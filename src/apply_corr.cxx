#include <iostream>
#include <ctime>
#include <getopt.h>

#include "baby_base.hpp"
#include "utilities.hpp"
#include "cross_sections.hpp"

#include "TError.h"

using namespace std;

namespace {
  string corrfile = "/net/cms29/cms29r0/babymaker/babies/2017_01_27/mc/corrections/corr_TTJets_SingleLeptFromT_Tune.root";
  string infile = "/net/cms29/cms29r0/babymaker/babies/2017_01_27/mc/unprocessed/fullbaby_TTJets_SingleLeptFromT_TuneCUETP8M1_13TeV-madgraphMLM-pythia8_RunIISummer16MiniAODv2-PUMoriond17_80X_mcRun2_asymptotic_2016_TrancheIV_v6_ext1-v1_33.root";
  string outfile = "/net/cms29/cms29r0/babymaker/babies/2017_01_27/mc/unskimmed/";
}

void GetOptions(int argc, char *argv[]);

int main(int argc, char *argv[]){
  // gErrorIgnoreLevel=6000; // Turns off ROOT errors due to missing branches       
  GetOptions(argc, argv);

  time_t begtime, endtime;
  time(&begtime);

  baby_base b(infile, outfile);
  long nent = b.GetEntries();
  cout<<"Running over "<<nent<<" events."<<endl;

  baby_base bcorr(corrfile,"",true);
  if (bcorr.GetEntries()==1) {
    cout<<"Correction file OK."<<endl;
    bcorr.GetEntry(0);
  } else {
    cout<<"No entries in the corrections files."<<endl;
  }

  bool isSignal = false;
  for(long entry(0); entry<nent; entry++){
    if (b.type()>100e3) isSignal = true;

    b.GetEntry(entry);
    if (entry%100000==0) {
      cout<<"Processing event: "<<entry<<endl;
    }

    b.out_baseline() = b.pass_ra2_badmu() && b.met()/b.met_calo()<5
                       && b.nleps()==1 && b.nveto()==0 && b.met()>200
                       && b.st()>500 && b.njets()>=6 && b.nbm()>=1;
    if (!isSignal) b.out_baseline() = b.out_baseline() && b.pass();

    if(b.nleps()==0) { // load from calculated correction
      b.out_w_lep()         = bcorr.w_lep();
      b.out_w_fs_lep()      = bcorr.w_fs_lep();
      for (unsigned i(0); i<b.sys_lep().size(); i++) 
        b.out_sys_lep()[i] = bcorr.sys_lep()[i];
      for (unsigned i(0); i<b.sys_fs_lep().size(); i++) 
        b.out_sys_fs_lep()[i] = bcorr.sys_fs_lep()[i];
    } else { //load from original tree
      b.out_w_lep()         = b.w_lep();
      b.out_w_fs_lep()      = b.w_fs_lep();
      for (unsigned i(0); i<b.sys_lep().size(); i++) 
        b.out_sys_lep()[i] = b.sys_lep()[i];
      for (unsigned i(0); i<b.sys_fs_lep().size(); i++) 
        b.out_sys_fs_lep()[i] = b.sys_fs_lep()[i];
    }

    b.out_w_lumi() = b.w_lumi()>0 ? 1. : -1.;
    b.out_w_lumi() *= bcorr.w_lumi();

    b.out_weight() = bcorr.weight() *b.out_w_lumi() 
                     *b.out_w_lep() *b.out_w_fs_lep() //post-corr values in order for 0l to be correct
                     *b.w_btag() *b.w_isr() *b.eff_jetid();
    
    b.out_w_isr() = bcorr.w_isr()*b.w_isr();
    for (unsigned i(0); i<b.sys_isr().size(); i++) 
      b.out_sys_isr()[i] = bcorr.sys_isr()[i]*b.sys_isr()[i];

    //      Cookie-cutter variables
    //-----------------------------------
    b.out_w_pu()                   = bcorr.w_pu()                  *b.w_pu();
    b.out_w_btag()                 = bcorr.w_btag()                *b.w_btag();
    b.out_w_btag_deep()            = bcorr.w_btag_deep()           *b.w_btag_deep();
    b.out_w_btag_loose()           = bcorr.w_btag_loose()          *b.w_btag_loose();
    b.out_w_btag_loose_deep()      = bcorr.w_btag_loose_deep()     *b.w_btag_loose_deep();
    b.out_w_btag_tight()           = bcorr.w_btag_tight()          *b.w_btag_tight();
    b.out_w_btag_tight_deep()      = bcorr.w_btag_tight_deep()     *b.w_btag_tight_deep();
    b.out_w_bhig()                 = bcorr.w_bhig()                *b.w_bhig();
    b.out_w_bhig_deep()            = bcorr.w_bhig_deep()           *b.w_bhig_deep();
    b.out_w_btag_proc()            = bcorr.w_btag_proc()           *b.w_btag_proc();
    b.out_w_btag_deep_proc()       = bcorr.w_btag_deep_proc()      *b.w_btag_deep_proc();
    b.out_w_btag_loose_proc()      = bcorr.w_btag_loose_proc()     *b.w_btag_loose_proc();
    b.out_w_btag_loose_deep_proc() = bcorr.w_btag_loose_deep_proc()*b.w_btag_loose_deep_proc();
    b.out_w_btag_tight_proc()      = bcorr.w_btag_tight_proc()     *b.w_btag_tight_proc();
    b.out_w_btag_tight_deep_proc() = bcorr.w_btag_tight_deep_proc()*b.w_btag_tight_deep_proc();
    b.out_w_bhig_proc()            = bcorr.w_bhig_proc()           *b.w_bhig_proc();
    b.out_w_bhig_deep_proc()       = bcorr.w_bhig_deep_proc()      *b.w_bhig_deep_proc();

    for (unsigned i(0); i<b.w_pdf().size(); i++)  b.out_w_pdf()[i] = bcorr.w_pdf()[i]*b.w_pdf()[i];

    for (unsigned i(0); i<b.sys_mur().size(); i++) {
      b.out_sys_mur()[i]                     = bcorr.sys_mur()[i]                    *b.sys_mur()[i];
      b.out_sys_muf()[i]                     = bcorr.sys_muf()[i]                    *b.sys_muf()[i];
      b.out_sys_murf()[i]                    = bcorr.sys_murf()[i]                   *b.sys_murf()[i];
      b.out_sys_pdf()[i]                     = bcorr.sys_pdf()[i]                    *b.sys_pdf()[i];
      b.out_sys_bctag()[i]                   = bcorr.sys_bctag()[i]                  *b.sys_bctag()[i];
      b.out_sys_bctag_deep()[i]              = bcorr.sys_bctag_deep()[i]             *b.sys_bctag_deep()[i];
      b.out_sys_udsgtag()[i]                 = bcorr.sys_udsgtag()[i]                *b.sys_udsgtag()[i];
      b.out_sys_udsgtag_deep()[i]            = bcorr.sys_udsgtag_deep()[i]           *b.sys_udsgtag_deep()[i];
      b.out_sys_bctag_loose()[i]             = bcorr.sys_bctag_loose()[i]            *b.sys_bctag_loose()[i];
      b.out_sys_bctag_loose_deep()[i]        = bcorr.sys_bctag_loose_deep()[i]       *b.sys_bctag_loose_deep()[i];
      b.out_sys_udsgtag_loose()[i]           = bcorr.sys_udsgtag_loose()[i]          *b.sys_udsgtag_loose()[i];
      b.out_sys_udsgtag_loose_deep()[i]      = bcorr.sys_udsgtag_loose_deep()[i]     *b.sys_udsgtag_loose_deep()[i];
      b.out_sys_bctag_tight()[i]             = bcorr.sys_bctag_tight()[i]            *b.sys_bctag_tight()[i];
      b.out_sys_bctag_tight_deep()[i]        = bcorr.sys_bctag_tight_deep()[i]       *b.sys_bctag_tight_deep()[i];
      b.out_sys_udsgtag_tight()[i]           = bcorr.sys_udsgtag_tight()[i]          *b.sys_udsgtag_tight()[i];
      b.out_sys_udsgtag_tight_deep()[i]      = bcorr.sys_udsgtag_tight_deep()[i]     *b.sys_udsgtag_tight_deep()[i];
      b.out_sys_bchig()[i]                   = bcorr.sys_bchig()[i]                  *b.sys_bchig()[i];
      b.out_sys_bchig_deep()[i]              = bcorr.sys_bchig_deep()[i]             *b.sys_bchig_deep()[i];
      b.out_sys_udsghig()[i]                 = bcorr.sys_udsghig()[i]                *b.sys_udsghig()[i];
      b.out_sys_udsghig_deep()[i]            = bcorr.sys_udsghig_deep()[i]           *b.sys_udsghig_deep()[i];
      b.out_sys_bctag_proc()[i]              = bcorr.sys_bctag_proc()[i]             *b.sys_bctag_proc()[i];
      b.out_sys_bctag_deep_proc()[i]         = bcorr.sys_bctag_deep_proc()[i]        *b.sys_bctag_deep_proc()[i];
      b.out_sys_udsgtag_proc()[i]            = bcorr.sys_udsgtag_proc()[i]           *b.sys_udsgtag_proc()[i];
      b.out_sys_udsgtag_deep_proc()[i]       = bcorr.sys_udsgtag_deep_proc()[i]      *b.sys_udsgtag_deep_proc()[i];
      b.out_sys_bctag_loose_proc()[i]        = bcorr.sys_bctag_loose_proc()[i]       *b.sys_bctag_loose_proc()[i];
      b.out_sys_bctag_loose_deep_proc()[i]   = bcorr.sys_bctag_loose_deep_proc()[i]  *b.sys_bctag_loose_deep_proc()[i];
      b.out_sys_udsgtag_loose_proc()[i]      = bcorr.sys_udsgtag_loose_proc()[i]     *b.sys_udsgtag_loose_proc()[i];
      b.out_sys_udsgtag_loose_deep_proc()[i] = bcorr.sys_udsgtag_loose_deep_proc()[i]*b.sys_udsgtag_loose_deep_proc()[i];
      b.out_sys_bctag_tight_proc()[i]        = bcorr.sys_bctag_tight_proc()[i]       *b.sys_bctag_tight_proc()[i];
      b.out_sys_bctag_tight_deep_proc()[i]   = bcorr.sys_bctag_tight_deep_proc()[i]  *b.sys_bctag_tight_deep_proc()[i];
      b.out_sys_udsgtag_tight_proc()[i]      = bcorr.sys_udsgtag_tight_proc()[i]     *b.sys_udsgtag_tight_proc()[i];
      b.out_sys_udsgtag_tight_deep_proc()[i] = bcorr.sys_udsgtag_tight_deep_proc()[i]*b.sys_udsgtag_tight_deep_proc()[i];
      b.out_sys_bchig_proc()[i]              = bcorr.sys_bchig_proc()[i]             *b.sys_bchig_proc()[i];
      b.out_sys_bchig_deep_proc()[i]         = bcorr.sys_bchig_deep_proc()[i]        *b.sys_bchig_deep_proc()[i];
      b.out_sys_udsghig_proc()[i]            = bcorr.sys_udsghig_proc()[i]           *b.sys_udsghig_proc()[i];
      b.out_sys_udsghig_deep_proc()[i]       = bcorr.sys_udsghig_deep_proc()[i]      *b.sys_udsghig_deep_proc()[i];
      b.out_sys_pu()[i]                      = bcorr.sys_pu()[i]                     *b.sys_pu()[i];
      if (isSignal) { // yes, this ignores the fullsim points
        b.out_sys_fs_bctag()[i]              = bcorr.sys_fs_bctag()[i]               *b.sys_fs_bctag()[i];
        b.out_sys_fs_bctag_deep()[i]         = bcorr.sys_fs_bctag_deep()[i]          *b.sys_fs_bctag_deep()[i];
        b.out_sys_fs_udsgtag()[i]            = bcorr.sys_fs_udsgtag()[i]             *b.sys_fs_udsgtag()[i];
        b.out_sys_fs_udsgtag_deep()[i]       = bcorr.sys_fs_udsgtag_deep()[i]        *b.sys_fs_udsgtag_deep()[i];
        b.out_sys_fs_bchig()[i]              = bcorr.sys_fs_bchig()[i]               *b.sys_fs_bchig()[i];
        b.out_sys_fs_bchig_deep()[i]         = bcorr.sys_fs_bchig_deep()[i]          *b.sys_fs_bchig_deep()[i];
        b.out_sys_fs_udsghig()[i]            = bcorr.sys_fs_udsghig()[i]             *b.sys_fs_udsghig()[i];
        b.out_sys_fs_udsghig_deep()[i]       = bcorr.sys_fs_udsghig_deep()[i]        *b.sys_fs_udsghig_deep()[i];
      }
    }

    b.Fill();

  } // loop over events
  
  b.Write();

  cout<<endl;
  time(&endtime); 
  cout<<"Time passed: "<<hoursMinSec(difftime(endtime, begtime))<<endl<<endl;  
}

void GetOptions(int argc, char *argv[]){
  while(true){
    static struct option long_options[] = {
      {"infile", required_argument, 0, 'i'},  // Method to run on (if you just want one)
      {"corrfile", required_argument, 0, 'c'},       // Apply correction
      {"outfile", required_argument, 0, 'o'},    // Luminosity to normalize MC with (no data)
      {0, 0, 0, 0}
    };

    char opt = -1;
    int option_index;
    opt = getopt_long(argc, argv, "i:c:o:", long_options, &option_index);
    if(opt == -1) break;

    string optname;
    switch(opt){
    case 'i':
      infile = optarg;
      break;
    case 'c':
      corrfile = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case 0:
    default:
      printf("Bad option! getopt_long returned character code 0%o\n", opt);
      break;
    }
  }
}