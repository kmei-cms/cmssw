#include "RecoParticleFlow/PFProducer/plugins/PFEGammaProducerNew.h"
#include "RecoParticleFlow/PFProducer/interface/PFEGammaAlgoNew.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyCalibration.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyCalibrationHF.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFSCEnergyCalibration.h"
#include "CondFormats/PhysicsToolsObjects/interface/PerformancePayloadFromTFormula.h"
#include "CondFormats/DataRecord/interface/PFCalibrationRcd.h"
#include "CondFormats/DataRecord/interface/GBRWrapperRcd.h"
#include "DataFormats/EgammaCandidates/interface/GsfElectronFwd.h"
#include "DataFormats/EgammaCandidates/interface/GsfElectron.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFwd.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementSuperClusterFwd.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementSuperCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockFwd.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlock.h"
#include "DataFormats/Common/interface/RefToPtr.h"
#include <sstream>

#include "TFile.h"

#ifdef PFLOW_DEBUG
#define docast(x,y) dynamic_cast<x>(y)
#define LOGVERB(x) edm::LogVerbatim(x)
#define LOGWARN(x) edm::LogWarning(x)
#define LOGERR(x) edm::LogError(x)
#define LOGDRESSED(x)  edm::LogInfo(x)
#else
#define docast(x,y) reinterpret_cast<x>(y)
#define LOGVERB(x) LogTrace(x)
#define LOGWARN(x) edm::LogWarning(x)
#define LOGERR(x) edm::LogError(x)
#define LOGDRESSED(x) LogDebug(x)
#endif

namespace {
  typedef std::list< reco::PFBlockRef >::iterator IBR;
}

PFEGammaProducerNew::PFEGammaProducerNew(const edm::ParameterSet& iConfig) {
  

  inputTagBlocks_ 
    = iConfig.getParameter<edm::InputTag>("blocks");

  eetopsSrc_ = iConfig.getParameter<edm::InputTag>("EEtoPS_source");

  usePhotonReg_
    =  iConfig.getParameter<bool>("usePhotonReg");

  useRegressionFromDB_
    = iConfig.getParameter<bool>("useRegressionFromDB"); 


  bool usePFSCEleCalib;
  std::vector<double>  calibPFSCEle_Fbrem_barrel; 
  std::vector<double>  calibPFSCEle_Fbrem_endcap;
  std::vector<double>  calibPFSCEle_barrel;
  std::vector<double>  calibPFSCEle_endcap;
  usePFSCEleCalib =     iConfig.getParameter<bool>("usePFSCEleCalib");
  calibPFSCEle_Fbrem_barrel = iConfig.getParameter<std::vector<double> >("calibPFSCEle_Fbrem_barrel");
  calibPFSCEle_Fbrem_endcap = iConfig.getParameter<std::vector<double> >("calibPFSCEle_Fbrem_endcap");
  calibPFSCEle_barrel = iConfig.getParameter<std::vector<double> >("calibPFSCEle_barrel");
  calibPFSCEle_endcap = iConfig.getParameter<std::vector<double> >("calibPFSCEle_endcap");
  std::shared_ptr<PFSCEnergyCalibration>  
    thePFSCEnergyCalibration ( new PFSCEnergyCalibration(calibPFSCEle_Fbrem_barrel,calibPFSCEle_Fbrem_endcap,
                                                         calibPFSCEle_barrel,calibPFSCEle_endcap )); 
                               
  bool useEGammaSupercluster = iConfig.getParameter<bool>("useEGammaSupercluster");
  double sumEtEcalIsoForEgammaSC_barrel = iConfig.getParameter<double>("sumEtEcalIsoForEgammaSC_barrel");
  double sumEtEcalIsoForEgammaSC_endcap = iConfig.getParameter<double>("sumEtEcalIsoForEgammaSC_endcap");
  double coneEcalIsoForEgammaSC = iConfig.getParameter<double>("coneEcalIsoForEgammaSC");
  double sumPtTrackIsoForEgammaSC_barrel = iConfig.getParameter<double>("sumPtTrackIsoForEgammaSC_barrel");
  double sumPtTrackIsoForEgammaSC_endcap = iConfig.getParameter<double>("sumPtTrackIsoForEgammaSC_endcap");
  double coneTrackIsoForEgammaSC = iConfig.getParameter<double>("coneTrackIsoForEgammaSC");
  unsigned int nTrackIsoForEgammaSC  = iConfig.getParameter<unsigned int>("nTrackIsoForEgammaSC");


  // register products
  produces<reco::PFCandidateCollection>();
  produces<reco::PFCandidateEGammaExtraCollection>();  
  produces<reco::SuperClusterCollection>();
  
  //PFElectrons Configuration
  double mvaEleCut
    = iConfig.getParameter<double>("pf_electron_mvaCut");

  
  std::string mvaWeightFileEleID
    = iConfig.getParameter<std::string>("pf_electronID_mvaWeightFile");

  bool applyCrackCorrectionsForElectrons
    = iConfig.getParameter<bool>("pf_electronID_crackCorrection");
  
  std::string path_mvaWeightFileEleID;

  path_mvaWeightFileEleID = edm::FileInPath ( mvaWeightFileEleID.c_str() ).fullPath();
     

  //PFPhoton Configuration

  std::string path_mvaWeightFileConvID;
  std::string mvaWeightFileConvID;
  std::string path_mvaWeightFileGCorr;
  std::string path_mvaWeightFileLCorr;
  std::string path_X0_Map;
  std::string path_mvaWeightFileRes;
  double mvaConvCut=-99.;
  double sumPtTrackIsoForPhoton = 99.;
  double sumPtTrackIsoSlopeForPhoton = 99.;


  mvaWeightFileConvID =iConfig.getParameter<std::string>("pf_convID_mvaWeightFile");
  mvaConvCut = iConfig.getParameter<double>("pf_conv_mvaCut");
  path_mvaWeightFileConvID = edm::FileInPath ( mvaWeightFileConvID.c_str() ).fullPath();  
  sumPtTrackIsoForPhoton = iConfig.getParameter<double>("sumPtTrackIsoForPhoton");
  sumPtTrackIsoSlopeForPhoton = iConfig.getParameter<double>("sumPtTrackIsoSlopeForPhoton");

  std::string X0_Map=iConfig.getParameter<std::string>("X0_Map");
  path_X0_Map = edm::FileInPath( X0_Map.c_str() ).fullPath();

  if(!useRegressionFromDB_) {
    std::string mvaWeightFileLCorr=iConfig.getParameter<std::string>("pf_locC_mvaWeightFile");
    path_mvaWeightFileLCorr = edm::FileInPath( mvaWeightFileLCorr.c_str() ).fullPath();
    std::string mvaWeightFileGCorr=iConfig.getParameter<std::string>("pf_GlobC_mvaWeightFile");
    path_mvaWeightFileGCorr = edm::FileInPath( mvaWeightFileGCorr.c_str() ).fullPath();
    std::string mvaWeightFileRes=iConfig.getParameter<std::string>("pf_Res_mvaWeightFile");
    path_mvaWeightFileRes=edm::FileInPath(mvaWeightFileRes.c_str()).fullPath();

    TFile *fgbr = new TFile(path_mvaWeightFileGCorr.c_str(),"READ");
    ReaderGC_  =(const GBRForest*)fgbr->Get("GBRForest");
    TFile *fgbr2 = new TFile(path_mvaWeightFileLCorr.c_str(),"READ");
    ReaderLC_  = (const GBRForest*)fgbr2->Get("GBRForest");
    TFile *fgbr3 = new TFile(path_mvaWeightFileRes.c_str(),"READ");
    ReaderRes_  = (const GBRForest*)fgbr3->Get("GBRForest");
    LogDebug("PFEGammaProducerNew")<<"Will set regressions from binary files " <<std::endl;
  }

  edm::ParameterSet iCfgCandConnector 
    = iConfig.getParameter<edm::ParameterSet>("iCfgCandConnector");


  // fToRead =  iConfig.getUntrackedParameter<std::vector<std::string> >("toRead");

  useCalibrationsFromDB_
    = iConfig.getParameter<bool>("useCalibrationsFromDB");    

  std::shared_ptr<PFEnergyCalibration> calibration(new PFEnergyCalibration()); 

  int algoType 
    = iConfig.getParameter<unsigned>("algoType");
  
  switch(algoType) {
  case 0:
    //pfAlgo_.reset( new PFAlgo);
    break;
   default:
    assert(0);
  }
  
  //PFEGamma
  setPFEGParameters(mvaEleCut,
		    path_mvaWeightFileEleID,
		    true,
		    thePFSCEnergyCalibration,
		    calibration,
		    sumEtEcalIsoForEgammaSC_barrel,
		    sumEtEcalIsoForEgammaSC_endcap,
		    coneEcalIsoForEgammaSC,
		    sumPtTrackIsoForEgammaSC_barrel,
		    sumPtTrackIsoForEgammaSC_endcap,
		    nTrackIsoForEgammaSC,
		    coneTrackIsoForEgammaSC,
		    applyCrackCorrectionsForElectrons,
		    usePFSCEleCalib,
		    useEGammaElectrons_,
		    useEGammaSupercluster,
		    true,
		    path_mvaWeightFileConvID,
		    mvaConvCut,
		    usePhotonReg_,
		    path_X0_Map,
		    sumPtTrackIsoForPhoton,
		    sumPtTrackIsoSlopeForPhoton);  

  //MIKE: Vertex Parameters
  vertices_ = iConfig.getParameter<edm::InputTag>("vertexCollection");

  verbose_ = 
    iConfig.getUntrackedParameter<bool>("verbose",false);

//   bool debug_ = 
//     iConfig.getUntrackedParameter<bool>("debug",false);

}



PFEGammaProducerNew::~PFEGammaProducerNew() {}

void 
PFEGammaProducerNew::beginRun(const edm::Run & run, 
                     const edm::EventSetup & es) 
{


  /*
  static map<std::string, PerformanceResult::ResultType> functType;

  functType["PFfa_BARREL"] = PerformanceResult::PFfa_BARREL;
  functType["PFfa_ENDCAP"] = PerformanceResult::PFfa_ENDCAP;
  functType["PFfb_BARREL"] = PerformanceResult::PFfb_BARREL;
  functType["PFfb_ENDCAP"] = PerformanceResult::PFfb_ENDCAP;
  functType["PFfc_BARREL"] = PerformanceResult::PFfc_BARREL;
  functType["PFfc_ENDCAP"] = PerformanceResult::PFfc_ENDCAP;
  functType["PFfaEta_BARREL"] = PerformanceResult::PFfaEta_BARREL;
  functType["PFfaEta_ENDCAP"] = PerformanceResult::PFfaEta_ENDCAP;
  functType["PFfbEta_BARREL"] = PerformanceResult::PFfbEta_BARREL;
  functType["PFfbEta_ENDCAP"] = PerformanceResult::PFfbEta_ENDCAP;
  */
  
  /*
  for(std::vector<std::string>::const_iterator name = fToRead.begin(); name != fToRead.end(); ++name) {    
    
    cout << "Function: " << *name << std::endl;
    PerformanceResult::ResultType fType = functType[*name];
    pfCalibrations->printFormula(fType);
    
    // evaluate it @ 10 GeV
    float energy = 10.;
    
    BinningPointByMap point;
    point.insert(BinningVariables::JetEt, energy);
    
    if(pfCalibrations->isInPayload(fType, point)) {
      float value = pfCalibrations->getResult(fType, point);
      cout << "   Energy before:: " << energy << " after: " << value << std::endl;
    } else cout <<  "outside limits!" << std::endl;
    
  }
  */
  
  if(useRegressionFromDB_) {
    edm::ESHandle<GBRForest> readerPFLCEB;
    edm::ESHandle<GBRForest> readerPFLCEE;    
    edm::ESHandle<GBRForest> readerPFGCEB;
    edm::ESHandle<GBRForest> readerPFGCEEHR9;
    edm::ESHandle<GBRForest> readerPFGCEELR9;
    edm::ESHandle<GBRForest> readerPFRes;
    es.get<GBRWrapperRcd>().get("PFLCorrectionBar",readerPFLCEB);
    ReaderLCEB_=readerPFLCEB.product();
    es.get<GBRWrapperRcd>().get("PFLCorrectionEnd",readerPFLCEE);
    ReaderLCEE_=readerPFLCEE.product();
    es.get<GBRWrapperRcd>().get("PFGCorrectionBar",readerPFGCEB);       
    ReaderGCBarrel_=readerPFGCEB.product();
    es.get<GBRWrapperRcd>().get("PFGCorrectionEndHighR9",readerPFGCEEHR9);
    ReaderGCEndCapHighr9_=readerPFGCEEHR9.product();
    es.get<GBRWrapperRcd>().get("PFGCorrectionEndLowR9",readerPFGCEELR9);
    ReaderGCEndCapLowr9_=readerPFGCEELR9.product();
    es.get<GBRWrapperRcd>().get("PFEcalResolution",readerPFRes);
    ReaderEcalRes_=readerPFRes.product();
    
    /*
    LogDebug("PFEGammaProducerNew")<<"setting regressions from DB "<<std::endl;
    */
  } 


  //pfAlgo_->setPFPhotonRegWeights(ReaderLC_, ReaderGC_, ReaderRes_);
  setPFPhotonRegWeights(ReaderLCEB_,ReaderLCEE_,ReaderGCBarrel_,ReaderGCEndCapHighr9_, ReaderGCEndCapLowr9_, ReaderEcalRes_ );
    
}


void 
PFEGammaProducerNew::produce(edm::Event& iEvent, 
			     const edm::EventSetup& iSetup) {
  
  LOGDRESSED("PFEGammaProducerNew")
    <<"START event: "
    <<iEvent.id().event()
    <<" in run "<<iEvent.id().run()<<std::endl;
  

  // reset output collection  
  egCandidates_.reset( new reco::PFCandidateCollection );   
  egExtra_.reset( new reco::PFCandidateEGammaExtraCollection ); 
  sClusters_.reset( new reco::SuperClusterCollection );    
  // copies of things for some reason?
  ebeeClusters_.reset( new reco::CaloClusterCollection );  
  esClusters_.reset( new reco::CaloClusterCollection );      
    
  // Get the EE-PS associations
  edm::Handle<reco::SuperCluster::EEtoPSAssociation> eetops;
  iEvent.getByLabel(eetopsSrc_,eetops);

  // Get The vertices from the event
  // and assign dynamic vertex parameters
  edm::Handle<reco::VertexCollection> vertices;
  bool gotVertices = iEvent.getByLabel(vertices_,vertices);
  if(!gotVertices) {
    std::ostringstream err;
    err<<"Cannot find vertices for this event.Continuing Without them ";
    edm::LogError("PFEGammaProducerNew")<<err.str()<<std::endl;
  }

  //Assign the PFAlgo Parameters
  setPFVertexParameters(useVerticesForNeutral_,vertices.product());

  // get the collection of blocks 

  edm::Handle< reco::PFBlockCollection > blocks;

  LOGDRESSED("PFEGammaProducerNew")<<"getting blocks"<<std::endl;
  bool found = iEvent.getByLabel( inputTagBlocks_, blocks );  

  if(!found ) {

    std::ostringstream err;
    err<<"cannot find blocks: "<<inputTagBlocks_;
    edm::LogError("PFEGammaProducerNew")<<err.str()<<std::endl;
    
    throw cms::Exception( "MissingProduct", err.str());
  }
  
  LOGDRESSED("PFEGammaProducerNew")
    <<"EGPFlow is starting..."<<std::endl;

#ifdef PFLOW_DEBUG
  assert( blocks.isValid() && "edm::Handle to blocks was null!");
  std::ostringstream  str;
  //str<<(*pfAlgo_)<<std::endl;
  //    cout << (*pfAlgo_) << std::endl;
  LOGDRESSED("PFEGammaProducerNew") <<str.str()<<std::endl;
#endif  

  // sort elements in three lists:
  std::list< reco::PFBlockRef > hcalBlockRefs;
  std::list< reco::PFBlockRef > ecalBlockRefs;
  std::list< reco::PFBlockRef > hoBlockRefs;
  std::list< reco::PFBlockRef > otherBlockRefs;
  
  for( unsigned i=0; i<blocks->size(); ++i ) {
    // reco::PFBlockRef blockref( blockh,i );
    //reco::PFBlockRef blockref = createBlockRef( *blocks, i);
    reco::PFBlockRef blockref(blocks, i);    
    
    const edm::OwnVector< reco::PFBlockElement >& 
      elements = blockref->elements();
   
    LOGDRESSED("PFEGammaProducerNew") 
      << "Found " << elements.size() 
      << " PFBlockElements in block: " << i << std::endl;
    
    bool singleEcalOrHcal = false;
    if( elements.size() == 1 ){
      switch( elements[0].type() ) {
      case reco::PFBlockElement::PS1:
      case reco::PFBlockElement::PS2:
      case reco::PFBlockElement::ECAL:
        ecalBlockRefs.push_back( blockref );
        singleEcalOrHcal = true;
	break;
      case reco::PFBlockElement::HCAL:
        hcalBlockRefs.push_back( blockref );
        singleEcalOrHcal = true;
	break;
      case reco::PFBlockElement::HO:
        // Single HO elements are likely to be noise. Not considered for now.
        hoBlockRefs.push_back( blockref );
        singleEcalOrHcal = true;
	break;
      default:
	break;
      }
    }
    
    if(!singleEcalOrHcal) {
      otherBlockRefs.push_back( blockref );
    }
  }//loop blocks
  
  // loop on blocks that are not single ecal, single ps1, single ps2 , or
  // single hcal and produce unbiased collection of EGamma Candidates

  //printf("loop over blocks\n");
  //unsigned nblcks = 0;

  // this auto is a const reco::PFBlockRef&
  for( const auto& blockref : otherBlockRefs ) {   
    // this auto is a: const edm::OwnVector< reco::PFBlockElement >&
    const auto& elements = blockref->elements();
    // make a copy of the link data, which will be edited.
    //PFBlock::LinkData linkData =  block.linkData();
    
    // keep track of the elements which are still active.
    std::vector<bool> active( elements.size(), true );      
    
    pfeg_->RunPFEG(blockref,active);
    
    const size_t egsize = egCandidates_->size();
    egCandidates_->resize(egsize + pfeg_->getCandidates().size());
    reco::PFCandidateCollection::iterator eginsertfrom = 
      egCandidates_->begin() + egsize;
    std::move(pfeg_->getCandidates().begin(),
	      pfeg_->getCandidates().end(),
	      eginsertfrom);
    
    const size_t egxsize = egExtra_->size();
    egExtra_->resize(egxsize + pfeg_->getEGExtra().size());
    reco::PFCandidateEGammaExtraCollection::iterator egxinsertfrom = 
      egExtra_->begin() + egxsize;
    std::move(pfeg_->getEGExtra().begin(),
	      pfeg_->getEGExtra().end(),
	      egxinsertfrom);

    const size_t rscsize = sClusters_->size();
    sClusters_->resize(egxsize + pfeg_->getRefinedSCs().size());
    reco::SuperClusterCollection::iterator rscinsertfrom = 
      sClusters_->begin() + rscsize;
    std::move(pfeg_->getRefinedSCs().begin(),
	      pfeg_->getRefinedSCs().end(),
	      rscinsertfrom);

    LOGDRESSED("PFEGammaProducerNew")
      << "post algo: egCandidates size = " 
      << egCandidates_->size() << std::endl;
  }
  
  edm::RefProd<reco::SuperClusterCollection> sClusterProd = 
    iEvent.getRefBeforePut<reco::SuperClusterCollection>();

  edm::RefProd<reco::PFCandidateEGammaExtraCollection> egXtraProd = 
    iEvent.getRefBeforePut<reco::PFCandidateEGammaExtraCollection>();
  
  //set the correct references to refined SC and EG extra using the refprods
  for (unsigned int i=0; i < egCandidates_->size(); ++i) {
    reco::PFCandidate &cand = egCandidates_->at(i);
    reco::PFCandidateEGammaExtra &xtra = egExtra_->at(i);
    
    reco::SuperClusterRef refinedSCRef(sClusterProd,i);
    reco::PFCandidateEGammaExtraRef extraref(egXtraProd,i);
    
    xtra.setSuperClusterRef(refinedSCRef); 
    cand.setSuperClusterRef(refinedSCRef);
    cand.setPFEGammaExtraRef(extraref);    
  }
  // release our demonspawn into the wild to cause havoc
  iEvent.put(sClusters_);
  iEvent.put(egExtra_);  
  iEvent.put(egCandidates_); 
}

//PFEGammaAlgo: a new method added to set the parameters for electron and photon reconstruction. 
void 
PFEGammaProducerNew::setPFEGParameters(double mvaEleCut,
				       std::string mvaWeightFileEleID,
                           bool usePFElectrons,
                           const std::shared_ptr<PFSCEnergyCalibration>& thePFSCEnergyCalibration,
                           const std::shared_ptr<PFEnergyCalibration>& thePFEnergyCalibration,
                           double sumEtEcalIsoForEgammaSC_barrel,
                           double sumEtEcalIsoForEgammaSC_endcap,
                           double coneEcalIsoForEgammaSC,
                           double sumPtTrackIsoForEgammaSC_barrel,
                           double sumPtTrackIsoForEgammaSC_endcap,
                           unsigned int nTrackIsoForEgammaSC,
                           double coneTrackIsoForEgammaSC,
                           bool applyCrackCorrections,
                           bool usePFSCEleCalib,
                           bool useEGElectrons,
                           bool useEGammaSupercluster,
                           bool usePFPhotons,  
                           std::string mvaWeightFileConvID, 
                           double mvaConvCut,
                           bool useReg,
                           std::string X0_Map,
                           double sumPtTrackIsoForPhoton,
                           double sumPtTrackIsoSlopeForPhoton                      
                        ) {
  
  mvaEleCut_ = mvaEleCut;
  usePFElectrons_ = usePFElectrons;
  applyCrackCorrectionsElectrons_ = applyCrackCorrections;  
  usePFSCEleCalib_ = usePFSCEleCalib;
  thePFSCEnergyCalibration_ = thePFSCEnergyCalibration;
  useEGElectrons_ = useEGElectrons;
  useEGammaSupercluster_ = useEGammaSupercluster;
  sumEtEcalIsoForEgammaSC_barrel_ = sumEtEcalIsoForEgammaSC_barrel;
  sumEtEcalIsoForEgammaSC_endcap_ = sumEtEcalIsoForEgammaSC_endcap;
  coneEcalIsoForEgammaSC_ = coneEcalIsoForEgammaSC;
  sumPtTrackIsoForEgammaSC_barrel_ = sumPtTrackIsoForEgammaSC_barrel;
  sumPtTrackIsoForEgammaSC_endcap_ = sumPtTrackIsoForEgammaSC_endcap;
  coneTrackIsoForEgammaSC_ = coneTrackIsoForEgammaSC;
  nTrackIsoForEgammaSC_ = nTrackIsoForEgammaSC;


  if(!usePFElectrons_) return;
  mvaWeightFileEleID_ = mvaWeightFileEleID;
  FILE * fileEleID = fopen(mvaWeightFileEleID_.c_str(), "r");
  if (fileEleID) {
    fclose(fileEleID);
  }
  else {
    std::string err = "PFAlgo: cannot open weight file '";
    err += mvaWeightFileEleID;
    err += "'";
    throw std::invalid_argument( err );
  }
  
  usePFPhotons_ = usePFPhotons;

  //for MVA pass PV if there is one in the collection otherwise pass a dummy    
  reco::Vertex dummy;  
  if(useVertices_)  
    {  
      dummy = primaryVertex_;  
    }  
  else { // create a dummy PV  
    reco::Vertex::Error e;  
    e(0, 0) = 0.0015 * 0.0015;  
    e(1, 1) = 0.0015 * 0.0015;  
    e(2, 2) = 15. * 15.;  
    reco::Vertex::Point p(0, 0, 0);  
    dummy = reco::Vertex(p, e, 0, 0, 0);  
  }  
  // pv=&dummy;  
  //if(! usePFPhotons_) return;  
  FILE * filePhotonConvID = fopen(mvaWeightFileConvID.c_str(), "r");  
  if (filePhotonConvID) {  
    fclose(filePhotonConvID);  
  }  
  else {  
    std::string err = "PFAlgo: cannot open weight file '";  
    err += mvaWeightFileConvID;  
    err += "'";  
    throw std::invalid_argument( err );  
  }  
  const reco::Vertex* pv=&dummy;  
  pfeg_.reset(new PFEGammaAlgoNew(mvaEleCut_,mvaWeightFileEleID_,
				  thePFSCEnergyCalibration_,
				  thePFEnergyCalibration,
				  applyCrackCorrectionsElectrons_,
				  usePFSCEleCalib_,
				  useEGElectrons_,
				  useEGammaSupercluster_,
				  sumEtEcalIsoForEgammaSC_barrel_,
				  sumEtEcalIsoForEgammaSC_endcap_,
				  coneEcalIsoForEgammaSC_,
				  sumPtTrackIsoForEgammaSC_barrel_,
				  sumPtTrackIsoForEgammaSC_endcap_,
				  nTrackIsoForEgammaSC_,
				  coneTrackIsoForEgammaSC_,
				  mvaWeightFileConvID, 
				  mvaConvCut, 
				  useReg,
				  X0_Map,  
				  *pv,
				  sumPtTrackIsoForPhoton,
				  sumPtTrackIsoSlopeForPhoton
				  ));
  return;  
  
//   pfele_= new PFElectronAlgo(mvaEleCut_,mvaWeightFileEleID_,
//                           thePFSCEnergyCalibration_,
//                           thePFEnergyCalibration,
//                           applyCrackCorrectionsElectrons_,
//                           usePFSCEleCalib_,
//                           useEGElectrons_,
//                           useEGammaSupercluster_,
//                           sumEtEcalIsoForEgammaSC_barrel_,
//                           sumEtEcalIsoForEgammaSC_endcap_,
//                           coneEcalIsoForEgammaSC_,
//                           sumPtTrackIsoForEgammaSC_barrel_,
//                           sumPtTrackIsoForEgammaSC_endcap_,
//                           nTrackIsoForEgammaSC_,
//                           coneTrackIsoForEgammaSC_);
}

/*
void PFAlgo::setPFPhotonRegWeights(
                  const GBRForest *LCorrForest,
                  const GBRForest *GCorrForest,
                  const GBRForest *ResForest
                  ) {                                                           
  if(usePFPhotons_) 
    pfpho_->setGBRForest(LCorrForest, GCorrForest, ResForest);
} 
*/
void PFEGammaProducerNew::setPFPhotonRegWeights(
                                   const GBRForest *LCorrForestEB,
                                   const GBRForest *LCorrForestEE,
                                   const GBRForest *GCorrForestBarrel,
                                   const GBRForest *GCorrForestEndcapHr9,
                                   const GBRForest *GCorrForestEndcapLr9,                                          const GBRForest *PFEcalResolution
                                   ){
  
  pfeg_->setGBRForest(LCorrForestEB,LCorrForestEE,
                       GCorrForestBarrel, GCorrForestEndcapHr9, 
                       GCorrForestEndcapLr9, PFEcalResolution);
}

void
PFEGammaProducerNew::setPFVertexParameters(bool useVertex,
                              const reco::VertexCollection*  primaryVertices) {
  useVertices_ = useVertex;

  //Set the vertices for muon cleaning
//  pfmu_->setInputsForCleaning(primaryVertices);


  //Now find the primary vertex!
  //bool primaryVertexFound = false;
  int nVtx=primaryVertices->size();
  pfeg_->setnPU(nVtx);
//   if(usePFPhotons_){
//     pfpho_->setnPU(nVtx);
//   }
  primaryVertex_ = primaryVertices->front();
  for (unsigned short i=0 ;i<primaryVertices->size();++i)
    {
      if(primaryVertices->at(i).isValid()&&(!primaryVertices->at(i).isFake()))
        {
          primaryVertex_ = primaryVertices->at(i);
          //primaryVertexFound = true;
          break;
        }
    }
  
  pfeg_->setPhotonPrimaryVtx(primaryVertex_ );
  
}
