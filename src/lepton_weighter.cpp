#include "lepton_weighter.hpp"

#include <string>
#include <numeric>

#include "TFile.h"
#include "TGraphAsymmErrors.h"

#include "utilities.hpp"

using namespace std;

namespace{
  template<typename T>
    T LoadSF(const string &file_name, const string &item_name){
    string path = "data/"+file_name;
    TFile f(path.c_str(), "read");
    if(!f.IsOpen()) ERROR("Could not open "+file_name);
    T* item = static_cast<T*>(f.Get(item_name.c_str()));
    if(!item) ERROR("Could not find "+item_name+" in "+file_name);
    return *item;
  }
  template<typename T>
    pair<double, double> GetSF(const T &h, double x, double y, bool ignore_error = false){
    pair<double, double> sf;
    auto bin = h.FindFixBin(x, y);
    if((h.IsBinOverflow(bin) || h.IsBinUnderflow(bin))
       && h.GetBinContent(bin) == 0. && h.GetBinError(bin) == 0.){
      auto bin_x = h.GetXaxis()->FindFixBin(x);
      auto bin_y = h.GetYaxis()->FindFixBin(y);
      if(bin_x <= 0) bin_x = 1;
      if(bin_x > h.GetNbinsX()) bin_x = h.GetNbinsX();
      if(bin_y <= 0) bin_y = 1;
      if(bin_y > h.GetNbinsY()) bin_y = h.GetNbinsY();
      sf = {h.GetBinContent(bin_x, bin_y), h.GetBinError(bin_x, bin_y)};
    }else{
      sf = {h.GetBinContent(bin), h.GetBinError(bin)};
    }
    if(ignore_error) sf.second = 0.;
    return sf;
  }

  pair<double, double> MergeSF(pair<double, double> a,
                               pair<double, double> b){
    double sf = a.first * b.first;
    double err = hypot(a.first*b.second, b.first*a.second);
    return {sf, err};
  }

  TH2D GraphToHist(const TGraphAsymmErrors &g){
    struct Point{
      double xl, xh, y, e;
      Point(double xl_in, double xh_in, double y_in, double e_in):
        xl(xl_in),
        xh(xh_in),
        y(y_in),
        e(e_in){
      }
      bool operator<(const Point &p) const{
        return make_tuple(xl, xh, fabs(log(fabs(y))), fabs(e))
          <make_tuple(p.xl, p.xh, fabs(log(fabs(p.y))), fabs(p.e));
      }
    };
    vector<Point> bins;
    Double_t *x = g.GetX();
    Double_t *xl = g.GetEXlow();
    Double_t *xh = g.GetEXhigh();
    Double_t *y = g.GetY();
    Double_t *yl = g.GetEYlow();
    Double_t *yh = g.GetEYhigh();
    for(int i = 0; i < g.GetN(); ++i){
      bins.emplace_back(x[i]-fabs(xl[i]), x[i]+fabs(xh[i]),
                        y[i], max(fabs(yl[i]), fabs(yh[i])));
    }
    bool problems = true;
    while(problems){
      stable_sort(bins.begin(), bins.end());
      problems = false;
      for(auto low = bins.begin(); !problems && low != bins.end(); ++low){
        auto high = low;
        ++high;
        if(high == bins.end()) break;
        double new_y = sqrt(low->y * high->y);
        double top = max(low->y+low->e, high->y+high->e);
        double bot = min(low->y-low->e, high->y-high->e);
        double new_e = max(top-new_y, new_y-bot);
        if(low->xh < high->xl){
          //Gap
          bins.insert(high, Point(low->xh, high->xl, new_y, new_e));
        }else if(low->xh > high->xl){
          //Overlap
          problems = true;
          if(low->xh < high->xh){
            //Plain overlap
            Point new_low(low->xl, high->xl, low->y, low->e);
            Point new_mid(high->xl, low->xh, new_y, new_e);
            Point new_high(low->xh, high->xh, high->y, high->e);
            *low = new_low;
            *high = new_high;
            bins.insert(high, new_mid);
          }else if(low->xh == high->xh){
            //Subset -> 2 bins
            Point new_low(low->xl, high->xl, low->y, low->e);
            Point new_high(high->xl, high->xh, new_y, new_e);
            *low = new_low;
            *high = new_high;
          }else{
            //Subset -> 3 bins
            Point new_low(low->xl, high->xl, low->y, low->e);
            Point new_mid(high->xl, high->xh, new_y, new_e);
            Point new_high(high->xh, low->xh, low->y, low->e);
            *low = new_low;
            *high = new_high;
            bins.insert(high, new_mid);
          }
        }
      }
    }
    vector<double> bin_edges(bins.size()+1);
    for(size_t i = 0; i < bins.size(); ++i){
      bin_edges.at(i) = bins.at(i).xl;
    }
    bin_edges.back() = bins.back().xh;
    TH2D h(g.GetName(), (string(g.GetTitle())+";"+g.GetXaxis()->GetTitle()+";"+g.GetYaxis()->GetTitle()).c_str(),
           1, 0., 1.e4, bin_edges.size()-1, &bin_edges.at(0));
    for(int ix = 0; ix <= 2; ++ix){
      h.SetBinContent(ix, 0, 1.);
      h.SetBinError(ix, 0, 1.);
      h.SetBinContent(ix, h.GetNbinsY()+1, 1.);
      h.SetBinError(ix, h.GetNbinsY()+1, 1.);
      for(int iy = 1; iy <= h.GetNbinsY(); ++iy){
        h.SetBinContent(ix, iy, bins.at(iy-1).y);
        h.SetBinError(ix ,iy, bins.at(iy-1).e);
      }
    }
    return h;
  }
}

//https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#Muons_AN1
const TH2F LeptonWeighter::sf_full_muon_medium_ = LoadSF<TH2F>("TnP_NUM_MediumID_DENOM_generalTracks_VAR_map_pt_eta.root",
                                                               "SF");
const TH2F LeptonWeighter::sf_full_muon_iso_ = LoadSF<TH2F>("TnP_NUM_MiniIsoTight_DENOM_MediumID_VAR_map_pt_eta.root",
                                                            "SF");

const TH2F LeptonWeighter::sf_full_muon_vtx_ = LoadSF<TH2F>("TnP_NUM_MediumIP2D_DENOM_LooseID_VAR_map_pt_eta.root",
                                                            "SF");


//Need to add muon tracking SF if it becomes available
const TH2D LeptonWeighter::sf_full_muon_tracking_ = GraphToHist(LoadSF<TGraphAsymmErrors>("sf_full_muon_tracking.root",
                                                                                          "ratio_eta"));

//https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#Electrons_AN1
const TH2F LeptonWeighter::sf_full_electron_medium_ = LoadSF<TH2F>("sf_full_electron_ID_and_iso_25_01_2017.root",
                                                                   "GsfElectronToCutBasedSpring15M");
const TH2F LeptonWeighter::sf_full_electron_iso_ = LoadSF<TH2F>("sf_full_electron_ID_and_iso_25_01_2017.root",
                                                                "MVAVLooseElectronToMini");
const TH2F LeptonWeighter::sf_full_electron_tracking_ = LoadSF<TH2F>("egammaEffi_EGM2D.root",
                                                                     "EGamma_SF2D");


const TH2D LeptonWeighter::sf_fast_muon_medium_ = LoadSF<TH2D>("sf_fast_muon_medium.root",
                                                               "histo2D");
const TH2D LeptonWeighter::sf_fast_muon_iso_ = LoadSF<TH2D>("sf_fast_muon_iso.root",
                                                            "histo2D");
const TH2D LeptonWeighter::sf_fast_electron_mediumiso_ = LoadSF<TH2D>("sf_fast_electron_mediumiso.root",
                                                                      "histo2D");

void LeptonWeighter::FullSim(baby_plus &b, float &w_lep, vector<float> &sys_lep){
  pair<double, double> sf(1., 0.);
  for(size_t i = 0; i < b.mus_sig().size(); ++i){
    if(b.mus_sig().at(i)){
      sf = MergeSF(sf, GetMuonScaleFactor(b, i));
    }
  }
  for(size_t i = 0; i < b.els_sig().size(); ++i){
    if(b.els_sig().at(i)){
      sf = MergeSF(sf, GetElectronScaleFactor(b, i));
    }
  }
  w_lep = sf.first;
  sys_lep = vector<float>{static_cast<float>(sf.first+sf.second),
                          static_cast<float>(sf.first-sf.second)};
}

void LeptonWeighter::FastSim(baby_plus &b, float &w_fs_lep, vector<float> &sys_fs_lep){
  pair<double, double> sf(1., 0.);
  for(size_t i = 0; i < b.mus_sig().size(); ++i){
    if(b.mus_sig().at(i)){
      sf = MergeSF(sf, GetMuonScaleFactorFS(b, i));
    }
  }
  for(size_t i = 0; i < b.els_sig().size(); ++i){
    if(b.els_sig().at(i)){
      sf = MergeSF(sf, GetElectronScaleFactorFS(b, i));
    }
  }
  w_fs_lep = sf.first;
  sys_fs_lep = vector<float>{static_cast<float>(sf.first+sf.second),
                             static_cast<float>(sf.first-sf.second)};
}

std::pair<double, double> LeptonWeighter::GetMuonScaleFactor(baby_plus &b, size_t imu){
  //https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#Data_leading_order_FullSim_MC_co
  //ID, iso, tracking SFs applied
  //No stat error, 3% systematic from ID, iso
  double pt = b.mus_pt().at(imu);
  double eta = b.mus_eta().at(imu);
  double abseta = fabs(eta);
  vector<pair<double, double> > sfs{
    GetSF(sf_full_muon_medium_, pt, abseta, false),
      make_pair(1., 0.03),//Systematic uncertainty
      GetSF(sf_full_muon_iso_, pt, abseta, false),
      make_pair(1., 0.03),//Systematic uncertainty
      GetSF(sf_full_muon_vtx_, pt, abseta, false),
      make_pair(1., 0.03)//Systematic uncertainty
      //GetSF(sf_full_muon_tracking_, pt, eta)//Asymmetric in eta
      };
  return accumulate(sfs.cbegin(), sfs.cend(), make_pair(1., 0.), MergeSF);
}

std::pair<double, double> LeptonWeighter::GetElectronScaleFactor(baby_plus &b, size_t iel){
  //https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#Data_leading_order_FullSim_M_AN1
  //ID, iso, tracking SFs applied
  //ID iso systematics built-in
  //Tracking SFs from https://twiki.cern.ch/twiki/bin/view/CMS/EgammaIDRecipesRun2#Electron_efficiencies_and_scale
  //3% tracking systematic below 20 GeV
  double pt = b.els_scpt().at(iel);
  double eta = b.els_sceta().at(iel);
  double abseta = fabs(eta);
  vector<pair<double, double> > sfs{
    GetSF(sf_full_electron_medium_, pt, abseta),
      GetSF(sf_full_electron_iso_, pt, abseta),
      GetSF(sf_full_electron_tracking_, eta, pt),//Axes swapped, asymmetric in eta
      //make_pair(1., pt<20. ? 0.03 : 0.)//Systematic uncertainty
      make_pair(1., pt<20. || pt >80. ? 0.01 : 0.)//Systematic uncertainty
      };
  return accumulate(sfs.cbegin(), sfs.cend(), make_pair(1., 0.), MergeSF);
}

std::pair<double, double> LeptonWeighter::GetMuonScaleFactorFS(baby_plus &b, size_t imu){
  //https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#FullSim_FastSim_TTBar_MC_compari
  //ID, iso SFs applied
  //No stat error, 2% systematic from ID, iso
  double pt = b.mus_pt().at(imu);
  double abseta = fabs(b.mus_eta().at(imu));
  vector<pair<double, double> > sfs{
    GetSF(sf_fast_muon_medium_, pt, abseta, false),
      make_pair(1., 0.02),
      GetSF(sf_fast_muon_iso_, pt, abseta, false),
      make_pair(1., 0.02),
      };
  return accumulate(sfs.cbegin(), sfs.cend(), make_pair(1., 0.), MergeSF);
}

std::pair<double, double> LeptonWeighter::GetElectronScaleFactorFS(baby_plus &b, size_t iel){
  //https://twiki.cern.ch/twiki/bin/view/CMS/SUSLeptonSF#FullSim_FastSim_TTBar_MC_com_AN1
  //ID, iso SFs applied
  //No stat error, 2% systematic from ID, iso
  double pt = b.els_scpt().at(iel);
  double abseta = fabs(b.els_sceta().at(iel));
  vector<pair<double, double> > sfs{
    GetSF(sf_fast_electron_mediumiso_, pt, abseta, false),
      make_pair(1., 0.02),//Systematic uncertainty
      make_pair(1., 0.02)//Systematic uncertainty
      };
  return accumulate(sfs.cbegin(), sfs.cend(), make_pair(1., 0.), MergeSF);
}

