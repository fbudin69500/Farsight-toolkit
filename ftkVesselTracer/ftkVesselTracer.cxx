
#include "ftkVesselTracer.h"

ftkVesselTracer::ftkVesselTracer(){
}

ftkVesselTracer::ftkVesselTracer(std::string input_data_path, bool preprocess = false, bool start_with_mst = false, bool use_vesselness = true){

	this->useVesselness = use_vesselness;

	if(preprocess){
		this->allParams.preProcessingParams.initByDefaultValues();
		ImageType3D::Pointer tiff_data_ptr; //only required for preprocessing
		this->PreprocessData(input_data_path, tiff_data_ptr); // ask user	
	}

	//testing
	//this->PrintToySWCFile();

	this->data_folder_path = input_data_path;
	this->data_folder_path.erase(this->data_folder_path.length()-4, this->data_folder_path.length());
	
	// Load preprocessed data
	this->LoadPreprocessedData(input_data_path);

	if(!start_with_mst){

		this->allParams.oriBin.initByDefaultValues();
		this->SphericalBinPreprocess();

		// 3D rendering of original and preprocessed data
		//Common::RescaleDataForRendering(this->originalData, this->originalDataForRendering);
		Common::RescaleDataForRendering(this->inputData, this->inputDataForRendering);		
		//std::cout << "Done with preparing data for rendering. " << std::endl; 
		//Common::RenderImage3D(this->originalDataForRendering);
		//Common::RenderImage3D(this->inputDataForRendering);
		

		// MIP computation
		//this->ComputeIntensityProjectionImages();
		//std::cout << "Done with preparing data for rendering. " << std::endl; 
		//this->RenderMaximumProjectionImage();	

		// Primary and secondary node detection
		this->allParams.nodeDetectionParams.initByDefaultValues();
		this->ComputeAllPrimaryNodes();
		this->ComputeAllSecondaryNodes();

		// MST and post processing
		this->allParams.graphAndMSTParams.initByDefaultValues();
		this->CreateMinimumSpanningForest();		
	}
	else{

		//Common::RescaleDataForRendering(this->originalData, this->originalDataForRendering);
		Common::RescaleDataForRendering(this->inputData, this->inputDataForRendering);
		
		// Reading secondary nodes from file for now
		std::string filename = "AllNodes_grid10.txt";
		this->ReadNodesFromTextFile(filename);
		//this->ReadNodesFromTextFile(std::string("AllNodes_default.txt"));
		
		this->allParams.graphAndMSTParams.initByDefaultValues();
		this->CreateMinimumSpanningForest();
	}
	
	this->PopulateSWCNodeContainerAndComputeNodeFeatures();
	this->WriteNodePropertiesFile();
	this->WriteSWCFileVessel();
	this->WriteSkeletonImage();
	this->WriteSegmentationMask();

	// "Smart" retracing
	this->SmartRetrace();
}
ftkVesselTracer::~ftkVesselTracer(){
}

void PreprocessingParameters::initByDefaultValues(void){
	
	this->medianFilterRadius = 1;
	this->anisDiffusionNIter = 10;
	this->anisDiffusionConductance = 30;
	this->smoothingSigma = 2.0f;
	this->iterNGVF = 30; //30; //10;

	VesselnessMeasures();
}

VesselnessMeasures::VesselnessMeasures(){

	this->alpha = 0.5;
	this->beta = 0.5;
	this->gamma = 0.025; //0.1;  //0.25;
	
	this->sigma_min = 1;
	this->sigma_max = 10.0;
	this->sigma_intervals = 10;
	this->vesselness_type = 1;

	this->ballness = 0.0;
	this->plateness = 0.0;
	this->vesselness = 0.0;
	this->noiseness = 0.0;
}

VesselnessMeasures::VesselnessMeasures(float alpha, float beta, float gamma){

	this->alpha = alpha;
	this->beta = beta;
	this->gamma = gamma;

	this->sigma_min = 0.5;
	this->sigma_max = 2.0;
	this->sigma_intervals = 1;
	this->vesselness_type = 1;

	this->ballness = 0.0;
	this->plateness = 0.0;
	this->vesselness = 0.0;
	this->noiseness = 0.0;
}

VesselnessMeasures::VesselnessMeasures(float sigma_min, float sigma_max, float sigma_intervals, int obj_type){

	this->alpha = 0.5;
	this->beta = 0.5;
	this->gamma = 0.25;

	this->sigma_min = sigma_min;
	this->sigma_max = sigma_max;
	this->sigma_intervals = sigma_intervals;
	this->vesselness_type = obj_type;

	this->ballness = 0.0;
	this->plateness = 0.0;
	this->vesselness = 0.0;
	this->noiseness = 0.0;
}
void SphericalBinInfo::initByDefaultValues(void){
	
	this->indexLength = 50;
	this->angleCount = 182;
	this->angleInrement = 10;
	this->nLastIndicesOfInterest = 8;
	this->histSmoothingFactor = 0.05;
	this->minSphHistCount = 0.0001;
}

void NodeDetectionParameters::initByDefaultValues(void){
	
	this->gridSpacing = 20; //30; //20; //30; //10; //15; //20; //15;
	this->iterNPrimaryNode = 100;

	this->increaseLikelihoodThreshold = 0.001;
	this->discardNodeLikelihoodThreshold = 0.01; // 0.001 accroding to thesis
	this->iterNForOnlyRegionBasedTerm = 15;
	this->iterNMinimum = 10;
	this->regionBasedTermWeight = 0.3; // 30%
	this->edgeBasedTermWeight = 0.7; // 70%
	this->minimumAccumulatedParamChange = 0.1;
	this->iterNMonitorParamChange = 5;
	this->currentMonitoredIter = 0;

	this->dtX = 5000.0; //50000; 
	this->dtY = 5000.0; //50000; 
	this->dtZ = 5000.0; //50000;
	this->dtScale = 1000.0; //10000;
	this->dirX = 0; this->dirY = 0; this->dirZ = 0;
	this->dirScale = 0;
	this->chX = std::vector<double>(this->iterNMonitorParamChange, 0.0); 
	this->chY = std::vector<double>(this->iterNMonitorParamChange, 0.0); 
	this->chZ = std::vector<double>(this->iterNMonitorParamChange, 0.0);
	this->chScale = std::vector<double>(this->iterNMonitorParamChange, 0.0);
	
	this->maxVesselWidth = 20;
	this->minVesselWidth = 2; //1;
	this->likelihoodThresholdPrimary = 0.04; //0.1; //0.01; //0.005;
	this->distanceThresholdPrimary = 1.2;

	this->traceQualityThreshold = 4.0; //3.0; //4.0; //5.0; // IMP PARAM
	this->maxQueueIter = 20000;
	this->distanceThresholdSecondary = 1.0; //0.5; // IMP PARAM
	this->maxBranchAngle = 60.0; //60.0; // in degrees // IMP PARAM
	this->branchingThreshold = 0.1;
	this->secondaryParamRateReduction = 1000.0;
	this->dtXSecondary = this->dtX / this->secondaryParamRateReduction;
	this->dtYSecondary = this->dtY / this->secondaryParamRateReduction;
	this->dtZSecondary = this->dtZ / this->secondaryParamRateReduction;
	this->dtScaleSecondary = this->dtScale / this->secondaryParamRateReduction;
	this->iterNMinimumSecondary = 15;
	this->regionBasedTermWeightSecondary = 0.5;
	this->edgeBasedTermWeightSecondary = 0.5;
	this->secondarySearchConstraint = std::sqrt(2.0);
	this->primaryReversePositionRate = 0.5;
	this->primaryReverseScaleRate = 0.5;
	this->secondaryReversePositionRate = 0.8;
	this->secondaryReverseScaleRate = 0.9;
	this->maxTraceCost = 10.0;
	this->infTraceQuality = 100.0;
	this->maxQueueSize = 10000;
	this->traceLengthCost = 0.5; //0.1; //0.5; //0.5; //1.0; //0.5; //1.0; //0.5; //0.0; // IMP PARAM
	this->primaryNodeSearchRadFactor = 0.75; //1.0; //0.5; // IMP PARAM

	this->vesselnessThershold = 0.001; //0.0001;
	this->vesselnessWeight = 0.3;
}

VesselNetworkFeatures::VesselNetworkFeatures(){

	this->totalVesselLength = 0.0;
	this->totalVesselVolume = 0.0;
	this->totalVesselness = 0.0;
	this->nBifurgations = 0;
	this->nTrifurgations = 0;
	this->nMultifurfations = 0;
	this->meanRadius = 0.0;
	this->meanVesselness = 0.0;
	this->vesselVolumePercentage = 0.0;
	
	this->totalTortuosity = 0.0;
	this->meanTortuosity = 0.0;
	this->nLoops = 0;
	this->collaterality = 0.0;
	this->meanLoopRadius = 0.0;
}

void GraphAndMSTPartameters::initByDefaultValues(void){

	this->affinityRadThresh = 2.0; //3.0; //2.5;
	this->NBinsAffinity = 32;
	this->maxEdgeWeight = 99.0;
	this->minBranchAngle = vnl_math::pi/8.0;
	this->maxNBranches = 40; //25; //10; //5;
	this->maxTreeNodes = 500; //100;
}
void AllParameters::initByDefaultValues(void){
	
	this->preProcessingParams.initByDefaultValues();
	this->oriBin.initByDefaultValues();
	this->nodeDetectionParams.initByDefaultValues();
	this->graphAndMSTParams.initByDefaultValues();
}

int ftkVesselTracer::PreprocessData(std::string file_path, ImageType3D::Pointer& data_ptr){

	itk::TimeProbe timer; // Timer for measuring the preprocessing time
	timer.Start();
	
	std::string write_file_path = file_path.substr(0, file_path.find_last_of('.'));
	std::string append_str = "_original.mhd";

	try{
		Common::ReadImage3D(file_path, data_ptr);

		//Common::WriteImage3D(write_file_path + std::string(append_str), data_ptr);
	}
	catch(itk::ExceptionObject& e){
		std::cout << e << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << "Done with reading input image. " << std::endl;

	//PARAMS: Make available outside function
	int median_radius = this->allParams.preProcessingParams.medianFilterRadius;
	int anis_diffusion_N_iter = this->allParams.preProcessingParams.anisDiffusionNIter;
	int anis_diffusion_conductance = this->allParams.preProcessingParams.anisDiffusionConductance;
	float smoothing_sigma = this->allParams.preProcessingParams.smoothingSigma;
	int GVF_N_iter = this->allParams.preProcessingParams.iterNGVF;
	
	Common::MedianFilter(median_radius, data_ptr);
	std::cout << "Done with median filtering. " << std::endl;

	Common::CurvatureAnisotropicDiffusion(anis_diffusion_N_iter, anis_diffusion_conductance, data_ptr);
	std::cout << "Done with anisotropic diffusion. " << std::endl;

	append_str = "_pre.tif";

	try{
		Common::WriteTIFFImage3D(write_file_path + std::string(append_str), data_ptr);
	}
	catch(itk::ExceptionObject& e){
		std::cout << e << std::endl;
		return EXIT_FAILURE;
	}

	Common::GVFDiffusionFaster(smoothing_sigma, GVF_N_iter, write_file_path, data_ptr);	
	std::cout << "Done with GVF diffusion. " << std::endl;

	//Compute vesselness image
	this->ComputeVesselnessImage(this->allParams.preProcessingParams.vesselness_measures, write_file_path, data_ptr);
	

	timer.Stop();
	std::cout << "The pre-processing took " << timer.GetMeanTime() << " seconds. " << std::endl;

	return EXIT_SUCCESS;
}

void ftkVesselTracer::LoadPreprocessedData(std::string data_path){
	
	data_path = data_path.substr(0, data_path.find_last_of('.')); // get rid of the extension
	std::string append_ext = "_pre.mhd";
	std::string append_original = ".mhd";
	std::string append_gx = "_gx.mhd";
	std::string append_gy = "_gy.mhd";
	std::string append_gz = "_gz.mhd";
	std::string append_vesselness = "_vesselness.mhd";

	try{
		Common::ReadImage3D(data_path + std::string(append_ext), this->inputData); //input data is preprocessed data

		//Common::ReadImage3D(data_path + std::string(append_original), this->originalData);	
		Common::ReadImage3D(data_path + std::string(append_gx), this->gx);
		Common::ReadImage3D(data_path + std::string(append_gy), this->gy);
		Common::ReadImage3D(data_path + std::string(append_gz), this->gz);
		Common::ReadImage3D(data_path + std::string(append_vesselness), this->VesselnessImage);
	}
	catch(itk::ExceptionObject& e){
		std::cout << e << std::endl;		
	}

	// Use the vesselness image instead of intensity image?
	//this->inputData = this->VesselnessImage;


	// Specify a region 
	

	//testing the data..
	/*MinMaxCalculatorType::Pointer min_max_filter = MinMaxCalculatorType::New();
	//min_max_filter->SetImage(this->inputData);
	//min_max_filter->SetImage(this->originalData);
	min_max_filter->SetImage(this->gx);
	min_max_filter->Compute();

	float min = min_max_filter->GetMinimum();
	float max = min_max_filter->GetMaximum();*/

	/*InvertImageFilterType::Pointer image_inverter = InvertImageFilterType::New();
	image_inverter->SetInput(this->originalData);
	image_inverter->Update();
	
	this->originalData = image_inverter->GetOutput();*/
	
	
	// Same pixel apparently has different values in ITK and Matlab!!
	/*ImageType3D::IndexType test_index, test_index1;
	test_index[0] = 123; test_index[1] = 6; test_index[2] = 23;
	test_index1[0] = 6; test_index1[1] = 123; test_index1[2] = 23;
	PixelType test_pixel = this->inputData->GetPixel(test_index);
	PixelType test_pixel1 = this->inputData->GetPixel(test_index1);
	std::cout << test_pixel << ", " << test_pixel1 << std::endl;
	*/
		
}

void ftkVesselTracer::ComputeIntensityProjectionImages(void){
	
	MaxProjectionFilterType::Pointer max_intensity_projector = MaxProjectionFilterType::New();
	max_intensity_projector->SetInput(this->inputDataForRendering);

	max_intensity_projector->Update();

	maximumProjectionImage = max_intensity_projector->GetOutput();

	MinProjectionFilterType::Pointer min_intensity_projector = MinProjectionFilterType::New();
	min_intensity_projector->SetInput(this->inputDataForRendering);

	min_intensity_projector->Update();

	this->minimumProjectionImage = min_intensity_projector->GetOutput();


	//CANNOT RETURN VTK IMAGE AFTER USING ITKTOVTK FILTER
	/*RenderImageType3D::Pointer max_intensity_image = max_intensity_projector->GetOutput();

	ITKToVTKConnectorType::Pointer itk_to_vtk_connector = ITKToVTKConnectorType::New();
	
	//itk_to_vtk_connector->SetInput(max_intensity_image);
	itk_to_vtk_connector->SetInput(this->inputDataForRendering);
	
	itk_to_vtk_connector->Update();

	//vtk_image = vtkSmartPointer<vtkImageData>::New();
	//this->MIP_image = itk_to_vtk_connector->GetOutput();

	//vtkSmartPointer<vtkImageActor> vtk_image_actor = vtkSmartPointer<vtkImageActor>::New();

	vtkSmartPointer<vtkImageData> vtk_image = itk_to_vtk_connector->GetOutput();

	MIP_image_actor = vtkSmartPointer<vtkImageActor>::New();
	
	MIP_image_actor->SetInput(vtk_image);
	MIP_image_actor->Update();

	//vtk_image->UpdateInformation();

	//testing projection image
	//vtk_image->PrintSelf(std::cout, vtkIndent(0));

	vtkSmartPointer<vtkImageViewer2> image_viewer = vtkSmartPointer<vtkImageViewer2>::New();
	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	image_viewer->SetInput(MIP_image_actor->GetInput()); // CODE BREAKS HERE
	image_viewer->SetupInteractor(render_window_interactor);
	image_viewer->Render();
	image_viewer->GetRenderer()->ResetCamera();
	render_window_interactor->Start();

	return vtk_image;*/
}

void ftkVesselTracer::RenderMaximumProjectionImage(void){

	ITKToVTKConnectorType::Pointer itk_to_vtk_connector = ITKToVTKConnectorType::New();
	
	//itk_to_vtk_connector->SetInput(max_intensity_image);
	itk_to_vtk_connector->SetInput(this->maximumProjectionImage);
	
	itk_to_vtk_connector->Update();

	// The new rendering pipeline
	vtkSmartPointer<vtkImageViewer2> image_viewer = vtkSmartPointer<vtkImageViewer2>::New();
	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();

	#if VTK_MAJOR_VERSION <= 5
		image_viewer->SetInput(itk_to_vtk_connector->GetOutput());
	#else
		image_viewer->SetInputData(itk_to_vtk_connector->GetOutput()); 
	#endif
	image_viewer->SetupInteractor(render_window_interactor);
	image_viewer->Render();
	image_viewer->GetRenderer()->ResetCamera();
	render_window_interactor->Start();
	
	// The traditional rendering pipeline
	/*vtkSmartPointer<vtkImageActor> image_actor = vtkSmartPointer<vtkImageActor>::New();
	
	image_actor->SetInput(vtk_image);
	 
	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderer->AddActor(image_actor);
	renderer->SetBackground(1.0, 1.0, 1.0);
	renderer->ResetCamera();
	
	vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
	render_window->AddRenderer(renderer);
	render_window->SetSize(500, 500);

	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	render_window_interactor->SetRenderWindow(render_window);
	
	render_window->Render();
	
	render_window_interactor->Initialize();
	render_window->Render();

	render_window_interactor->Start();	
	*/
}

bool CompareArrayElementsPreserveIndex2(arrayElement a1, arrayElement a2){
	return(a1.getElement() < a2.getElement());
}

void ftkVesselTracer::SphericalBinPreprocess(void){

	std::cout << "Computing SphericalBinInfo..." << std::endl;

	int indexLength = this->allParams.oriBin.indexLength;
	int angleCount = this->allParams.oriBin.angleCount;

	int dim_size = 2 * indexLength + 1;
	
	//VectorType3D temp(dim_size, VectorType2D(dim_size, VectorType1D(dim_size, 0)));
	
	this->allParams.oriBin.binIndexVec = VectorType3D(dim_size, VectorType2D(dim_size, VectorType1D(dim_size, 0)));
	
	std::vector<double> theta, phi, x(angleCount, 0), y(angleCount, 0), z(angleCount, -1);
	
	for(int i = -4; i <= 4; i++)
		theta.push_back(vnl_math::pi * (double)i / 10.0);
	for(int i = -9; i <= 10; i++)	
		phi.push_back(vnl_math::pi * (double)i / 10.0);
	
	//Calculating bin centers
	int count = 1;
	
	//#pragma omp parallel for firstprivate(count)
	for(int i = 0; i < theta.size(); i++){
		for(int j = 0; j < phi.size(); j++){
			x[count] = cos(double(theta[i])) * cos(double(phi[j]));
			y[count] = cos(double(theta[i])) * sin(double(phi[j]));
			z[count] = sin(double(theta[i]));

			count++;
		}
	}
	z[count] = 1;

	// A small test array
	/*double test_point[3] = {0.0, 0.0, 0.0}, test_norm = 1.0;
	int test_max_d_position = 0;
	double test_max_d = 0.0;
	std::vector<double> test_d(x.size(), 1);
	VectorType3D test_vec = VectorType3D(11, VectorType2D(11, VectorType1D(11, 0)));
	for(int i1 = -5; i1 <=5; i1++){
		for(int i2 = -5; i2 <= 5; i2++){
			for(int i3 = -5; i3 <=5; i3++){

				test_point[0] = (double)i1; test_point[1] = (double)i2; test_point[2] = (double)i3;
				test_norm = sqrt((test_point[0]*test_point[0]) + (test_point[1]*test_point[1]) + (test_point[2]*test_point[2]));
				test_point[0] = test_point[0] / test_norm; test_point[1] = test_point[1] / test_norm; test_point[2] = test_point[2] / test_norm;
				
				// Trying to find which point in the cartesian space is closest to the current bin point
				for(int i = 0; i < x.size(); i++)
					test_d[i] = (test_point[0] * x[i]) + (test_point[1] * y[i]) + (test_point[2] * z[i]);

				test_max_d = *std::max_element(test_d.begin(), test_d.end());
				test_max_d_position = std::find(test_d.begin(), test_d.end(), test_max_d) - test_d.begin(); 
				
				test_vec[i3 + 5][i2 + 5][i1 + 5] = test_max_d_position;
			}
		}
	}
	int t1 = test_vec[3][5][6];
	int t2 = test_vec[5][2][9];
	*/

	#pragma omp parallel for 
	for(int i1 = -indexLength; i1 <= indexLength; i1++){
		for(int i2 = -indexLength; i2 <= indexLength; i2++){
			for(int i3 = -indexLength; i3 <= indexLength; i3++){
				
				double a_point[3] = {0.0, 0.0, 0.0}, norm = 1.0;
				a_point[0] = (double)i1; a_point[1] = (double)i2; a_point[2] = (double)i3;
				norm = sqrt((a_point[0]*a_point[0]) + (a_point[1]*a_point[1]) + (a_point[2]*a_point[2]));
				a_point[0] = a_point[0] / norm; a_point[1] = a_point[1] / norm; a_point[2] = a_point[2] / norm;
				
				// Trying to find which point in the cartesian space is closest to the current bin point
				std::vector<double> d(x.size(), 1);
				for(int i = 0; i < x.size(); i++)
					d[i] = (a_point[0] * x[i]) + (a_point[1] * y[i]) + (a_point[2] * z[i]);

				double max_d = *std::max_element(d.begin(), d.end());
				int max_d_position = std::find(d.begin(), d.end(), max_d) - d.begin(); 
				
				// CHECK THE VALUES HERE 
				
				//this->allParams.oriBin.binIndexVec[i1+indexLength+1][i2+indexLength+1][i3+indexLength+1] = max_d_position;
				
				//this->allParams.oriBin.binIndexVec[i3+indexLength][i2+indexLength][i1+indexLength] = max_d_position;
				this->allParams.oriBin.binIndexVec[i3+indexLength][i1+indexLength][i2+indexLength] = max_d_position;
				

				//this->allParams.oriBin.binIndexVec[i1+indexLength][i2+indexLength][i3+indexLength] = max_d_position;
				
				//this->allParams.oriBin.binIndexVec[i2+indexLength][i1+indexLength][i3+indexLength] = max_d_position;
			}
		}
	}

	int a1 = this->allParams.oriBin.binIndexVec[46][49][49];
	int a2 = this->allParams.oriBin.binIndexVec[49][46][49];
	
	std::vector<std::vector<double> > bin_centers(3, std::vector<double>(x.size(), 0));
	//bin_centers[0] = x;
	//bin_centers[1] = y;
	
	bin_centers[0] = y;
	bin_centers[1] = x;
	
	bin_centers[2] = z;
	this->allParams.oriBin.binCenters = bin_centers;
	
	std::vector<std::vector<double> > D(angleCount, std::vector<double>(angleCount, 0));
	std::vector<double> temp1(3, 0), temp2(3, 0);
	double acc = 0.0;
	for(int r = 0; r < angleCount; r++){
		for(int s = r+1; s < angleCount; s++){
			acc = 0.0;
			temp1[0] = bin_centers[0][r]; temp1[1] = bin_centers[1][r]; temp1[2] = bin_centers[2][r];
			temp2[0] = bin_centers[0][s]; temp2[1] = bin_centers[1][s]; temp2[2] = bin_centers[2][s];
			acc = std::inner_product(temp1.begin(), temp1.end(), temp2.begin(), acc); 

			D[r][s] = acc; D[s][r] = acc;
		}
	}
	
	// Using a map to sort numbers while preserving their index. The order for keys with same values might not be 
	// maintained after inserting in the map.
	int n_index_of_interest = this->allParams.oriBin.nLastIndicesOfInterest;
	VectorType2D nbr(angleCount, VectorType1D(n_index_of_interest, 0));

	/*VectorType1D D_sorted_index, D_sorted_index_of_interest;
	std::multimap<double, int> D_column_map;
	std::multimap<double, int>::iterator iter;
	for(int i = 0; i < D[0].size(); i++){
		
		for(int j = 0; j < D[0].size(); j++)
			D_column_map.insert(std::pair<double, int>(D[i][j], j));
		
		for(iter = D_column_map.begin(); iter != D_column_map.end(); iter++)
			D_sorted_index.push_back((*iter).second);
		
		std::reverse(D_sorted_index.begin(), D_sorted_index.end()); // for descending order
		
		nbr[i] = VectorType1D(D_sorted_index.begin(), D_sorted_index.begin() + n_index_of_interest);

		D_column_map.clear();
	}*/
	
	std::vector<arrayElement> D_col_vec;
	for(int i = 0; i < D.size(); i++){
	
		for(int j = 0; j < D[i].size(); j++)
			D_col_vec.push_back(arrayElement(D[i][j], j));
		
		std::stable_sort(D_col_vec.begin(), D_col_vec.end(), arrayElement::CompareArrayElementsDescendPreserveIndex);
		//std::stable_sort(D_col_vec.begin(), D_col_vec.end(), CompareArrayElementsPreserveIndex2);		
		//std::reverse(D_col_vec.begin(), D_col_vec.end());

		for(int j = 0; j < n_index_of_interest; j++)
			nbr[i][j] = D_col_vec[j].getIndex();
		
		D_col_vec.clear();
	}

	this->allParams.oriBin.nbr = nbr;	

	std::cout << "SphericalBinInfo computed." << std::endl;
}

void ftkVesselTracer::ComputeAllPrimaryNodes(void){
	
	this->globalStatsInput.volumeMax = Common::NormalizeData(this->inputData, this->normalizedInputData);
	this->ComputeSeeds();
	this->FitSphereAndSortNodes();
}

void ftkVesselTracer::ComputeSeeds(void){
	
	std::cout << "Computing initial seed points..." << std::endl;

	ImageType3D::SizeType size = this->normalizedInputData->GetBufferedRegion().GetSize();
	//ImageType3D::SizeType size = this->normalizedInputData->GetLargestPossibleRegion().GetSize();

	int grid_spacing = this->allParams.nodeDetectionParams.gridSpacing;

	//VolumeOfInterestFilterType::Pointer sub_volume_filter = VolumeOfInterestFilterType::New();
	//sub_volume_filter->SetInput(this->normalizedInputData);

	StatisticsFilterType::Pointer stats_filter = StatisticsFilterType::New();

	ImageType3D::IndexType starting_index, max_index, min_index;
	ImageType3D::SizeType sub_volume_size; //, size1;
	ImageType3D::RegionType sub_volume_region;
	ImageType3D::Pointer sub_volume;
	//MinMaxCalculatorType::Pointer min_max_calculator = MinMaxCalculatorType::New();

	
	int d1 = size[0], d2 = size[1], d3 = size[2];
	//int j1 = 0, j2 = 0, j3 = 0;
	//double max_val = 0, min_val = 0, pixel1 = 0, pixel2 = 0, pixel3 = 0;
	int seed_count = 0;

	#pragma omp parallel for private(starting_index, max_index, min_index, sub_volume_size, sub_volume_region, sub_volume) 
	for(int i1 = 0; i1 < d1; i1 = i1 + grid_spacing){
		for(int i2 = 0; i2 < d2; i2 = i2 + grid_spacing){
			for(int i3 = 0; i3 < d3; i3 = i3 + grid_spacing){
			
				int j1 = std::min(i1 + grid_spacing - 1, d1);
				int j2 = std::min(i2 + grid_spacing - 1, d2);
				int j3 = std::min(i3 + grid_spacing - 1, d3);
				
				//starting_index[0] = i2; starting_index[1] = i1; starting_index[2] = i3;
				//sub_volume_size[0] = j2 - i2; sub_volume_size[1] = j1 - i1; sub_volume_size[2] = j3 - i3;
				starting_index[0] = i1; starting_index[1] = i2; starting_index[2] = i3;
				sub_volume_size[0] = j1 - i1; sub_volume_size[1] = j2 - i2; sub_volume_size[2] = j3 - i3;
				
				sub_volume_region.SetIndex(starting_index);
				sub_volume_region.SetSize(sub_volume_size);
				
				VolumeOfInterestFilterType::Pointer sub_volume_filter = VolumeOfInterestFilterType::New();
				sub_volume_filter->SetInput(this->normalizedInputData);
				sub_volume_filter->SetRegionOfInterest(sub_volume_region);
				sub_volume_filter->Update();
				sub_volume = sub_volume_filter->GetOutput();
				
				// Node statistics can be computed here to reject some seeds which lie in the background
				/*stats_filter->SetInput(sub_volume);
				stats_filter->Update();
				float mean = stats_filter->GetMean();
				float std = stats_filter->GetSigma();
				float max = stats_filter->GetMaximum();
				float min = stats_filter->GetMinimum();

				/*size1 = sub_volume->GetBufferedRegion().GetSize();
				double pixel1 = this->normalizedInputData->GetPixel(starting_index);
			    double pixel2 = sub_volume->GetPixel(starting_index);
				double pixel3 = this->inputData->GetPixel(starting_index);*/
				
				MinMaxCalculatorType::Pointer min_max_calculator = MinMaxCalculatorType::New();
				min_max_calculator->SetImage(sub_volume);
				min_max_calculator->Compute();
				
				double max_val = min_max_calculator->GetMaximum();
				double min_val = min_max_calculator->GetMinimum();
				max_index = min_max_calculator->GetIndexOfMaximum();
				min_index = min_max_calculator->GetIndexOfMinimum();

				//this->initialSeeds[seed_count].x = max_index[0];
				//this->initialSeeds[seed_count].y = max_index[1];
				//this->initialSeeds[seed_count].z = max_index[2];
				
				//max_index = min_index;	
				
				#pragma omp critical
				this->initialSeeds.push_back(Node(max_index[0] + i1, max_index[1] + i2, max_index[2] + i3, max_val));
				
				//#pragma omp critical
				//seed_count++;
			}
		}
	}
	
	std::cout << "Seed points computed: " << this->initialSeeds.size() << std::endl;

	//visualizing the seed nodes
	//this->VisualizeNodesWithData3D(this->initialSeeds, false);
}

void ftkVesselTracer::VisualizeNodesWithData3D(std::vector<Node> node_vec, bool view_as_point){
	
	int n_seeds = node_vec.size();
	
	// Prepare the vtkVolume for rendering the data

	ITKToVTKConnectorType::Pointer ITK_to_VTK_connector = ITKToVTKConnectorType::New();

	ITK_to_VTK_connector->SetInput(this->inputDataForRendering);
	ITK_to_VTK_connector->Update();

	vtkSmartPointer<vtkImageData> vtk_image = ITK_to_VTK_connector->GetOutput();

	// Testing vtk image
	//vtk_image->PrintSelf(std::cout, vtkIndent(0));

	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderer->SetBackground(1.0, 1.0, 1.0);
	
	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();
	renderer->SetActiveCamera(camera);

	vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
	render_window->AddRenderer(renderer);
	
	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	render_window_interactor->SetRenderWindow(render_window);
	
	vtkSmartPointer<vtkPiecewiseFunction> opacity_transfer_function = vtkSmartPointer<vtkPiecewiseFunction>::New();
	opacity_transfer_function->AddPoint(2, 0.0);
	opacity_transfer_function->AddPoint(10, 0.1);
	
	vtkSmartPointer<vtkColorTransferFunction> color_transfer_function = vtkSmartPointer<vtkColorTransferFunction>::New();
	color_transfer_function->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
	color_transfer_function->AddRGBPoint((10 * 255)/100, 1, 1, 1); //blue
	color_transfer_function->AddRGBPoint((45 * 255)/100, 0, .01, 0); //green
	color_transfer_function->AddRGBPoint((150 * 255)/100, .01, 0, 0); //red
	
	vtkSmartPointer<vtkVolumeProperty> volume_property = vtkSmartPointer<vtkVolumeProperty>::New();
	volume_property->SetColor(color_transfer_function);
	volume_property->SetScalarOpacity(opacity_transfer_function);
	volume_property->ShadeOn();
	volume_property->SetInterpolationTypeToLinear();
	
	vtkSmartPointer<vtkFixedPointVolumeRayCastMapper> volume_mapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
	
	#if VTK_MAJOR_VERSION <= 5
		volume_mapper->SetInput(vtk_image);
	#else
		volume_mapper->SetInputData(vtk_image);
	#endif
	volume_mapper->SetBlendModeToComposite();
	
	vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
	volume->SetMapper(volume_mapper); 
	volume->SetProperty(volume_property);
	volume->SetPosition(0, 0, 0);
	volume->SetPickable(0);
	volume->Update();

	// Prepare vtkActors for each node. Every node is displayed as a sphere

	std::vector<vtkSmartPointer<vtkSphereSource> > node_spheres;
	std::vector<vtkSmartPointer<vtkPolyDataMapper> > node_mappers;
	std::vector<vtkSmartPointer<vtkActor> > node_actors;

	for(int i = 0; i < n_seeds; i++){
		
		vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
		vtkSmartPointer<vtkPolyDataMapper> poly_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
		vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();

		sphere->SetCenter(node_vec[i].x, node_vec[i].y, node_vec[i].z);

		if(view_as_point == true)
			sphere->SetRadius(1);
		else
			sphere->SetRadius(node_vec[i].scale);
		
		poly_mapper->SetInputConnection(sphere->GetOutputPort());
		actor->SetMapper(poly_mapper);
		actor->GetProperty()->SetColor(0, 1, 0); //green
		
		node_spheres.push_back(sphere);
		node_mappers.push_back(poly_mapper);
		node_actors.push_back(actor);		
	}
	

	for(int i = 0; i < n_seeds; i++)		
		renderer->AddActor(node_actors[i]);

	renderer->AddVolume(volume);

	renderer->ResetCamera();
	
	render_window_interactor->Initialize();
	render_window->Render();
	render_window_interactor->Start();
}

Node::Node(){

	this->x = 0;
	this->y = 0;
	this->z = 0;
	this->intensity = 0;
	this->scale = 1.0;
	this->likelihood = 0.0;
	this->isValid = true;
	this->nHoodScale = this->scale;
	this->bandNhood = this->scale;
	this->bandArea = 0;
	this->foregroundArea = 0;
	this->backgroundArea = 0;
	this->meanBackgroundIntensity = 0.0;
	this->meanForegroundIntensity = 255.0;
	this->exitIter = 0;
	this->vesselness = 0.0;

	this->traceQuality = 0.0;
	this->parentIDLength = 0;
	this->isPrimary = false;
	this->isSecondary = false;
	this->nHoodSecondaryMultiplier = 0.0;
	this->nHoodScaleSecondary = 0.0;
	
	this->secondaryNodeSearchRad = 0.0;
	this->xInitSecondary = 0.0;
	this->yInitSecondary = 0.0;
	this->zInitSecondary = 0.0;

	
	this->ID = -1;
	this->NBranches = 0;

	this->forestLabel = -3;
	this->isRoot = false;
	this->isLeaf = false;
	this->isBifurgation = false;
	this->isMultifurgation = false;
	this->isTrifurgation = false;
	this->isOrphan = false;
}

Node::Node(double x, double y, double z, PixelType intensity){

	this->x = x;
	this->y = y;
	this->z = z;
	this->intensity = intensity;

	this->scale = 1.0;
	this->likelihood = 0.0;
	this->isValid = true;
	this->nHoodScale = 2.0 * this->scale;
	this->bandNhood = 0.5;
	this->bandArea = 0;
	this->foregroundArea = 0;
	this->backgroundArea = 0;
	this->meanBackgroundIntensity = 0.0;
	this->meanForegroundIntensity = 255.0;
	this->exitIter = 0;
	this->vesselness = 0.0;

	this->traceQuality = 0.0;
	this->parentIDLength = 4;
	this->isPrimary = true;
	this->isSecondary = false;
	this->nHoodSecondaryMultiplier = 2.0;
	this->nHoodScaleSecondary = this->nHoodSecondaryMultiplier * this->scale;
	
	this->secondaryNodeSearchRad = 0.0;
	this->xInitSecondary = 0.0;
	this->yInitSecondary = 0.0;
	this->zInitSecondary = 0.0;


	this->ID = -1;
	this->NBranches = 0;

	this->forestLabel = -3;
	this->isRoot = false;
	this->isRoot = false;
	this->isLeaf = false;
	this->isBifurgation = false;
	this->isMultifurgation = false;
	this->isTrifurgation = false;
	this->isOrphan = false;
}

bool compareNodesByLikelihood(Node n1, Node n2){	
	return (n1.likelihood < n2.likelihood); 
}

inline double Node::ComputeNorm(Node n){

	return(std::sqrt((n.x * n.x) + (n.y * n.y) + (n.z * n.z)));
}
void ftkVesselTracer::FitSphereAndSortNodes(void){

	std::cout << "Processing the seeds based on the model fits, likelihood and distance... " << std::endl;

	//Testing a node
	//this->initialSeeds.clear();
	//Node ps_node(213, 16, 44, 100);
	//this->initialSeeds.push_back(ps_node);

	//seeds before fitting
	//this->VisualizeNodesWithData3D(this->initialSeeds, false);

	std::vector<Node> filteredPrimaryNodes;
	filteredPrimaryNodes.resize(this->initialSeeds.size());

	//#pragma omp parallel for 
	for(int i = 0; i < this->initialSeeds.size(); i++){
		this->FitSphereAtNode(this->initialSeeds[i]);
		//std::cout << i << " Scale: " << this->initialSeeds[i].scale << " Likelihood: " << this->initialSeeds[i].likelihood << " Last iter: " << this->initialSeeds[i].exitIter << std::endl; 
		
		//#pragma omp critical
			if((this->initialSeeds[i].isValid == true) && (this->initialSeeds[i].likelihood > this->allParams.nodeDetectionParams.likelihoodThresholdPrimary)){
				//this->primaryNodes.push_back(this->initialSeeds[i]);
				filteredPrimaryNodes[i] = this->initialSeeds[i];

				//std::cout << " Scale: " << this->initialSeeds[i].scale << " Likelihood: " << this->initialSeeds[i].likelihood << std::endl;
			}
	}

	for(int i = 0; i < filteredPrimaryNodes.size(); i++){
		if(filteredPrimaryNodes[i].likelihood > 0.0)
			this->primaryNodes.push_back(filteredPrimaryNodes[i]);
	}


	//for testing
	/*for(int i = 0; i < this->primaryNodes.size(); i++){
		std::cout << this->primaryNodes[i].x << ", " << this->primaryNodes[i].y << ", " << this->primaryNodes[i].z << ", ";
		std::cout << this->primaryNodes[i].likelihood << ", " << this->primaryNodes[i].scale << ", " << this->primaryNodes[i].meanForegroundIntensity << ", ";
		std::cout << this->primaryNodes[i].meanBackgroundIntensity << std::endl;
	}*/

	//visualize the fitted models and the filtered nodes
	//this->VisualizeNodesWithData3D(this->primaryNodes, false);

	std::cout << "Primary nodes before hit test: " << this->primaryNodes.size() << std::endl;

	this->SortAndFilterPrimaryNodes();
	
	//final primary nodes
	this->VisualizeNodesWithData3D(this->primaryNodesAfterHitTest, false);


	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->primaryNodesImage = RenderImageType3D::New();
	this->primaryNodesImage->SetRegions(id_reg);
	this->primaryNodesImage->Allocate();
	this->primaryNodesImage->SetSpacing(this->inputData->GetSpacing());

	this->primaryNodesImage->FillBuffer(0);

	itk::Index<3> idx;
	for(int i = 0; i < this->primaryNodesAfterHitTest.size(); i++){
		
		idx[0] = this->primaryNodesAfterHitTest[i].x;
		idx[1] = this->primaryNodesAfterHitTest[i].y;
		idx[2] = this->primaryNodesAfterHitTest[i].z;

		this->primaryNodesImage->SetPixel(idx, 255);
	}
	
	std::string primary_nodes_file_name = this->data_folder_path;
	primary_nodes_file_name.append("_PrimaryNodes.tif");

	ImageWriter::Pointer primary_nodes_writer = ImageWriter::New();	
	primary_nodes_writer->SetFileName(primary_nodes_file_name);	
	primary_nodes_writer->SetInput(this->primaryNodesImage);
	primary_nodes_writer->Update();

	std::cout << "Seed processing completed for primary nodes. " << std::endl;
	std::cout << "Number of primary nodes after hit test: " << this->primaryNodesAfterHitTest.size() << std::endl;
	
	// Testing the model fitting for specific seeds
	//std::vector<Node> testing_seeds(this->initialSeeds.begin()+500, this->initialSeeds.begin()+510);
	//std::vector<Node> testing_seeds(this->initialSeeds.begin()+292, this->initialSeeds.begin()+293);
	/*std::vector<Node> testing_seeds(this->initialSeeds.begin()+200, this->initialSeeds.begin()+201);
	this->VisualizeNodesWithData3D(testing_seeds, false);
	for(int i = 0; i < testing_seeds.size(); i++)
		this->FitSphereAtNode(testing_seeds[i]);
	//std::cout << " Scale: " << testing_seeds[i].scale << " Likelihood: " << testing_seeds[i].likelihood << << " Position: " << seed.x << ", " << seed.y << ", " << seed.z " Last iter: " << testing_seeds[i].exitIter << std::endl; 
	this->VisualizeNodesWithData3D(testing_seeds, false);
	*/	
}

void ftkVesselTracer::FitSphereAtNode(Node& seed){

	//testing on a pseudo node
	// Same pixel apparently has different values in ITK and Matlab!!
	/*seed.x = 21;
	seed.y = 17;
	seed.z = 16;
	ImageType3D::IndexType test_index;
	test_index[0] = seed.x; test_index[1] = seed.y; test_index[2] = seed.z;
	PixelType test_pixel = this->inputData->GetPixel(test_index);
	PixelType test_pixel2 = this->normalizedInputData->GetPixel(test_index);
	*/
	
	seed.InitDefaultParamsBeforeOptimization();
	
	// Very important step!! 
	//#pragma omp critical
	this->allParams.nodeDetectionParams.initByDefaultValues();

	int node_iter = this->allParams.nodeDetectionParams.iterNPrimaryNode;

	for(int i = 0; i < node_iter; i++){

		//testing
		//std::cout << seed.meanForegroundIntensity << ", " << seed.meanBackgroundIntensity << ", " << seed.likelihood << std::endl;
			
		this->UpdateAppearanceVectorized(seed);
		
		if(seed.isValid == false)
			break; 

		this->UpdateModel(seed, i);

		if(this->ExitModelFitting(seed, i) == true)
			break;
	}

}

void ftkVesselTracer::FitSphereAtNode(Node& seed, ImageType3D::Pointer data_ptr, ImageType3D::Pointer gx, ImageType3D::Pointer gy, ImageType3D::Pointer gz){

	//testing on a pseudo node
	// Same pixel apparently has different values in ITK and Matlab!!
	/*seed.x = 21;
	seed.y = 17;
	seed.z = 16;
	ImageType3D::IndexType test_index;
	test_index[0] = seed.x; test_index[1] = seed.y; test_index[2] = seed.z;
	PixelType test_pixel = this->inputData->GetPixel(test_index);
	PixelType test_pixel2 = this->normalizedInputData->GetPixel(test_index);
	*/
	
	seed.InitDefaultParamsBeforeOptimization();
		
	// Very important step!! 
	this->allParams.nodeDetectionParams.initByDefaultValues();

	int node_iter = this->allParams.nodeDetectionParams.iterNPrimaryNode;

	for(int i = 0; i < node_iter; i++){

		//std::cout << " Scale: " << seed.scale << " Position: " << seed.x << ", " << seed.y << ", " << seed.z << "  Likelihood: " << seed.likelihood << std::endl;
			
		this->UpdateAppearanceVectorized(seed, data_ptr, gx, gy, gz);
		
		if(seed.isValid == false)
			break; 

		this->UpdateModel(seed, i);

		if(this->ExitModelFitting(seed, i) == true)
			break;
	}
}

void Node::InitDefaultParamsBeforeOptimization(){
	
	this->scale = (double)Node::DEFALUT_SCALE;
	this->nHoodScale = 2 * this->scale;
	this->likelihood = (double)Node::MIN_LIKELIHOOD;
}

void ftkVesselTracer::FitSphereAtNodeSecondary(Node& primary_node, Node& secondary_node, std::vector<double> dir_vec){

	Node anchor_node;
	anchor_node.x = primary_node.x; 
	anchor_node.y = primary_node.y; 
	anchor_node.z = primary_node.z;
	
	double anchor_dir_norm = Node::ComputeNorm(Node(dir_vec[0], dir_vec[1], dir_vec[2], 0.0));
	anchor_node.dirX.push_back(dir_vec[0]/anchor_dir_norm);
	anchor_node.dirY.push_back(dir_vec[1]/anchor_dir_norm);	
	anchor_node.dirZ.push_back(dir_vec[2]/anchor_dir_norm);

	anchor_node.secondaryNodeSearchRad = this->allParams.nodeDetectionParams.primaryNodeSearchRadFactor * primary_node.scale;
	
	anchor_node.xInitSecondary = primary_node.x + anchor_node.dirX[0] * anchor_node.secondaryNodeSearchRad;
	anchor_node.yInitSecondary = primary_node.y + anchor_node.dirY[0] * anchor_node.secondaryNodeSearchRad;
	anchor_node.zInitSecondary = primary_node.z + anchor_node.dirZ[0] * anchor_node.secondaryNodeSearchRad;


	secondary_node.isSecondary = true;
	secondary_node.x = anchor_node.xInitSecondary;
	secondary_node.y = anchor_node.yInitSecondary;
	secondary_node.z = anchor_node.zInitSecondary;
	secondary_node.scale = primary_node.scale;
	secondary_node.nHoodScale = 2.0 * secondary_node.scale;
	secondary_node.dirX.clear();
	secondary_node.dirY.clear();
	secondary_node.dirZ.clear();
	secondary_node.meanForegroundIntensity = primary_node.meanForegroundIntensity;
	secondary_node.meanBackgroundIntensity = primary_node.meanBackgroundIntensity;
	secondary_node.likelihood = 0.0;
	secondary_node.bandNhood = 0.5; // FUNCTION CALL REQUIRED
	secondary_node.sphHistRegionBased = primary_node.sphHistRegionBased;

	//Very important step !!
	this->allParams.nodeDetectionParams.initByDefaultValues();
	this->allParams.nodeDetectionParams.dtX = this->allParams.nodeDetectionParams.dtXSecondary;
	this->allParams.nodeDetectionParams.dtY = this->allParams.nodeDetectionParams.dtYSecondary;
	this->allParams.nodeDetectionParams.dtZ = this->allParams.nodeDetectionParams.dtZSecondary;
	this->allParams.nodeDetectionParams.dtScale = this->allParams.nodeDetectionParams.dtScaleSecondary;

	int node_iter = this->allParams.nodeDetectionParams.iterNPrimaryNode;

	for(int i = 0; i < node_iter; i++){

		//std::cout << " Scale: " << secondary_node.scale << " Position: " << secondary_node.x << ", " << secondary_node.y << ", " << secondary_node.z << "  Likelihood: " << secondary_node.likelihood << std::endl;
		
		this->UpdateAppearanceVectorized(secondary_node);

		if(secondary_node.isValid == false)
			break;
		
		this->UpdateModelSecondary(secondary_node, anchor_node, i);

		if(this->ExitModelFitting(secondary_node, i) == true)
			break;
	}
}

void ftkVesselTracer::UpdateAppearanceVectorized(Node& seed){
	
	ImageType3D::IndexType seed_index;
	seed_index[0] = seed.x; seed_index[1] = seed.y; seed_index[2] = seed.z;
	ImageType3D::SizeType size = this->normalizedInputData->GetLargestPossibleRegion().GetSize();

	//if(this->normalizedInputData->GetBufferedRegion().IsInside(seed_index) == false)
	//if(this->normalizedInputData->GetLargestPossibleRegion().IsInside(seed_index) == false){
	if((seed.x < 0) || (seed.x > size[0]) || (seed.y < 0) || (seed.y > size[1]) || (seed.z < 0) || (seed.z > size[2])){
		seed.isValid = false;
		return;
	}
	
	ImageType3D::IndexType nhood_start_index, nhood_end_index;
	ImageType3D::SizeType nhood_size;
	ImageType3D::RegionType sub_vol;

	// VIS!
	seed.nHoodScale = 2.0 * seed.scale;
	
	nhood_start_index[0] = floor(seed.x - seed.nHoodScale + 0.5); 
	nhood_start_index[1] = floor(seed.y - seed.nHoodScale + 0.5); 
	nhood_start_index[2] = floor(seed.z - seed.nHoodScale + 0.5);
	nhood_end_index[0] = floor(seed.x + seed.nHoodScale + 0.5);
	nhood_end_index[1] = floor(seed.y + seed.nHoodScale + 0.5);
	nhood_end_index[2] = floor(seed.z + seed.nHoodScale + 0.5);

	nhood_size[0] = 2.0*seed.nHoodScale; nhood_size[1] = 2.0*seed.nHoodScale; nhood_size[2] = 2.0*seed.nHoodScale;
	
	/*VolumeOfInterestFilterType::Pointer sub_vol_filter = VolumeOfInterestFilterType::New();
	sub_vol_filter->SetInput(this->normalizedInputData);
	sub_vol.SetIndex(nhood_start_index);
	sub_vol.SetSize(nhood_size);
	sub_vol_filter->SetRegionOfInterest(sub_vol);
	sub_vol_filter->Update();
	*/
	
	seed.bandArea = 0;
	seed.foregroundArea = 0;
	seed.backgroundArea = 0;
	seed.meanForegroundIntensity = 0.0;
	seed.meanBackgroundIntensity = 0.0;
	seed.xNormalizedInBand.clear();
	seed.yNormalizedInBand.clear();
	seed.zNormalizedInBand.clear();
	seed.intensityInBand.clear();
	seed.gxInBand.clear();
	seed.gyInBand.clear();
	seed.gzInBand.clear(); 

	ImageType3D::IndexType current_index;
	PixelType x_normalized = 0.0, y_normalized = 0.0, z_normalized = 0.0, radial_dist = 0.0;
	bool is_inside_volume = true, is_outside_volume = false, is_on_foreground = true, is_on_background = false, contributes_to_energy = true, energy_band_outside_volume = false;
	int in_volume_count = 0, out_volume_count = 0;
	std::vector<double> foreground_values, background_values;
	
	for(int k = nhood_start_index[2]; k <= nhood_end_index[2]; k++){
		for(int i = nhood_start_index[0]; i <= nhood_end_index[0]; i++){
			for(int j = nhood_start_index[1]; j <= nhood_end_index[1]; j++){
			//for(int k = nhood_start_index[2]; k <= nhood_end_index[2]; k++){
				
				current_index[0] = i; current_index[1] = j; current_index[2] = k; 
				
				//if(this->normalizedInputData->GetBufferedRegion().IsInside(current_index) == true){
				//if(this->normalizedInputData->GetLargestPossibleRegion().IsInside(current_index) == true){
				if((i > 0) && (i < size[0]) && (j > 0) && (j < size[1]) && (k > 0) && (k < size[2])){
					is_inside_volume = true;
					in_volume_count++;
				}
				else{
					is_inside_volume = false;
					out_volume_count++;
				}
								
				x_normalized = ((double)i - seed.x)/seed.scale;
				y_normalized = ((double)j - seed.y)/seed.scale;
				z_normalized = ((double)k - seed.z)/seed.scale;
				radial_dist = std::sqrt((x_normalized*x_normalized) + (y_normalized*y_normalized) + (z_normalized*z_normalized));

				if((radial_dist - 1.0 <= 0.00001) && (is_inside_volume == true))
				//if(((radial_dist < 1.0) || (std::abs(radial_dist - 1.0) < 0.000001)) && (is_inside_volume == true))
					is_on_foreground = true;
				else
					is_on_foreground = false;

				if((is_on_foreground == false) && (is_inside_volume == true))
					is_on_background = true;
				else
					is_on_background = false;

				if(((double)abs(radial_dist - 1.0) < seed.bandNhood) && (is_inside_volume == true))		
					contributes_to_energy = true;
				else
					contributes_to_energy = false;

				if(((double)abs(radial_dist - 1.0) < seed.bandNhood) && (is_inside_volume == false))		
					energy_band_outside_volume = true;
				else
					energy_band_outside_volume = false;

				if(contributes_to_energy == true){
					seed.xNormalizedInBand.push_back(x_normalized/radial_dist);
					seed.yNormalizedInBand.push_back(y_normalized/radial_dist);
					seed.zNormalizedInBand.push_back(z_normalized/radial_dist);
					seed.intensityInBand.push_back(this->normalizedInputData->GetPixel(current_index));
					seed.gxInBand.push_back(this->gx->GetPixel(current_index));
					seed.gyInBand.push_back(this->gy->GetPixel(current_index));
					seed.gzInBand.push_back(this->gz->GetPixel(current_index));

					seed.bandArea++;
				}

				if(energy_band_outside_volume == true){
					seed.xNormalizedInBand.push_back(x_normalized/radial_dist);
					seed.yNormalizedInBand.push_back(y_normalized/radial_dist);
					seed.zNormalizedInBand.push_back(z_normalized/radial_dist);
					seed.intensityInBand.push_back(0);
					seed.gxInBand.push_back(0);
					seed.gyInBand.push_back(0);
					seed.gzInBand.push_back(0);
					
					seed.bandArea++;
				}
				
				if(is_on_foreground == true){

					//std::cout << "Position: " << i << ", " << j << ", " << k << " Value: " << this->normalizedInputData->GetPixel(current_index) << std::endl;
					
					foreground_values.push_back(this->normalizedInputData->GetPixel(current_index));
					
					seed.meanForegroundIntensity = seed.meanForegroundIntensity + this->normalizedInputData->GetPixel(current_index);
					seed.foregroundArea++;
				}
				if(is_on_background == true){

					background_values.push_back(this->normalizedInputData->GetPixel(current_index));

					seed.meanBackgroundIntensity = seed.meanBackgroundIntensity + this->normalizedInputData->GetPixel(current_index);
					seed.backgroundArea++;
				}

			} 
		}
	}

	if(seed.isSecondary == true){
		
		if(foreground_values.size() != 0){
			// Calculate the median
			std::sort(foreground_values.begin(), foreground_values.end());

			seed.meanForegroundIntensity = *(foreground_values.begin() + (foreground_values.size()/2));
		}
		else
			seed.meanForegroundIntensity = 0.0;

		if(background_values.size() != 0){
			std::sort(background_values.begin(), background_values.end());
			seed.meanBackgroundIntensity = *(background_values.begin() + (background_values.size()/2));
		}
		else
			seed.meanBackgroundIntensity = 0.0;
	}
	else{

		// Calculate the mean
		seed.meanForegroundIntensity = seed.meanForegroundIntensity / (float)seed.foregroundArea;
		seed.meanBackgroundIntensity = seed.meanBackgroundIntensity / (float)seed.backgroundArea;
	}

	seed.likelihood = seed.meanForegroundIntensity - seed.meanBackgroundIntensity;

	//testing
	//std::cout << seed.foregroundArea << ", " << seed.backgroundArea << ", ";
	//std::cout << seed.meanForegroundIntensity << ", " << seed.meanBackgroundIntensity << ", " << seed.likelihood << std::endl;

	// If likelood is less than 0.001, do what? According to the thesis, the subvolume should be discarded. But here, the likelihood value is
	// increased if it is less then 0.001
	/*double mean_of_means = 0.0;
	if(seed.isSecondary == true && seed.likelihood < this->allParams.nodeDetectionParams.increaseLikelihoodThreshold){
		
		mean_of_means = (seed.meanForegroundIntensity + seed.meanBackgroundIntensity) / 2.0;
		seed.meanForegroundIntensity = mean_of_means + this->allParams.nodeDetectionParams.discardNodeLikelihoodThreshold;
		seed.meanBackgroundIntensity = mean_of_means - this->allParams.nodeDetectionParams.discardNodeLikelihoodThreshold;
		if(seed.meanBackgroundIntensity < 0)
			seed.meanBackgroundIntensity = 0;
		seed.likelihood = seed.meanForegroundIntensity - seed.meanBackgroundIntensity;
	}*/
	
	// Set seed as invalid if majority of its neighbourhood lies outside the data limits
	if(in_volume_count < out_volume_count)
		seed.isValid = false;
	else
		seed.isValid = true;	
}

void ftkVesselTracer::UpdateAppearanceVectorized(Node &seed, ImageType3D::Pointer data_ptr, ImageType3D::Pointer gx, ImageType3D::Pointer gy, ImageType3D::Pointer gz){
		
	ImageType3D::IndexType seed_index;
	seed_index[0] = seed.x; seed_index[1] = seed.y; seed_index[2] = seed.z;
	ImageType3D::SizeType size = data_ptr->GetLargestPossibleRegion().GetSize();

	//if(data_ptr->GetBufferedRegion().IsInside(seed_index) == false)
	if(data_ptr->GetLargestPossibleRegion().IsInside(seed_index) == false)
	//if((seed.x > 0) && (seed.x < size[0]) && (seed.y > 0) && (seed.y < size[1]) && (seed.z > 0) && (seed.z < size[2]))
		seed.isValid = false;
	
	ImageType3D::IndexType nhood_start_index, nhood_end_index;
	ImageType3D::SizeType nhood_size;
	ImageType3D::RegionType sub_vol;

	// VIS!
	seed.nHoodScale = 2.0 * seed.scale;
	
	nhood_start_index[0] = floor(seed.x - seed.nHoodScale + 0.5); 
	nhood_start_index[1] = floor(seed.y - seed.nHoodScale + 0.5); 
	nhood_start_index[2] = floor(seed.z - seed.nHoodScale + 0.5);
	nhood_end_index[0] = floor(seed.x + seed.nHoodScale + 0.5);
	nhood_end_index[1] = floor(seed.y + seed.nHoodScale + 0.5);
	nhood_end_index[2] = floor(seed.z + seed.nHoodScale + 0.5);

	nhood_size[0] = 2*seed.nHoodScale; nhood_size[1] = 2*seed.nHoodScale; nhood_size[2] = 2*seed.nHoodScale;
	
	/*VolumeOfInterestFilterType::Pointer sub_vol_filter = VolumeOfInterestFilterType::New();
	sub_vol_filter->SetInput(data_ptr);
	sub_vol.SetIndex(nhood_start_index);
	sub_vol.SetSize(nhood_size);
	sub_vol_filter->SetRegionOfInterest(sub_vol);
	sub_vol_filter->Update();
	*/
	
	seed.bandArea = 0;
	seed.foregroundArea = 0;
	seed.backgroundArea = 0;
	seed.meanForegroundIntensity = 0;
	seed.meanBackgroundIntensity = 0;
	seed.xNormalizedInBand.clear();
	seed.yNormalizedInBand.clear();
	seed.zNormalizedInBand.clear();
	seed.intensityInBand.clear();
	seed.gxInBand.clear();
	seed.gyInBand.clear();
	seed.gzInBand.clear();

	ImageType3D::IndexType current_index;
	PixelType x_normalized = 0, y_normalized = 0, z_normalized = 0, radial_dist = 0;
	bool is_inside_volume = true, is_outside_volume = false, is_on_foreground = true, is_on_background = false, contributes_to_energy = true, energy_band_outside_volume = false;
	int in_volume_count = 0, out_volume_count = 0;
	std::vector<double> foreground_values, background_values;
	
	for(int k = nhood_start_index[2]; k <= nhood_end_index[2]; k++){
		for(int i = nhood_start_index[0]; i <= nhood_end_index[0]; i++){
			for(int j = nhood_start_index[1]; j <= nhood_end_index[1]; j++){
			//for(int k = nhood_start_index[2]; k <= nhood_end_index[2]; k++){
				
				current_index[0] = i; current_index[1] = j; current_index[2] = k; 
				
				//if(data_ptr->GetBufferedRegion().IsInside(current_index) == true){
				if(data_ptr->GetLargestPossibleRegion().IsInside(current_index) == true){
				//if((i > 0) && (i < size[0]) && (j > 0) && (j < size[1]) && (k > 0) && (k < size[2])){
					is_inside_volume = true;
					in_volume_count++;
				}
				else{
					is_inside_volume = false;
					out_volume_count++;
				}
								
				x_normalized = (i - seed.x)/seed.scale;
				y_normalized = (j - seed.y)/seed.scale;
				z_normalized = (k - seed.z)/seed.scale;
				radial_dist = std::sqrt((x_normalized*x_normalized) + (y_normalized*y_normalized) + (z_normalized*z_normalized));

				if((radial_dist <= 1.0) && (is_inside_volume == true))
				//if(((radial_dist < 1.0) || (std::abs(radial_dist - 1.0) < 0.000001)) && (is_inside_volume == true))
					is_on_foreground = true;
				else
					is_on_foreground = false;

				if((is_on_foreground == false) && (is_inside_volume == true))
					is_on_background = true;
				else
					is_on_background = false;

				if((abs(radial_dist - 1) < seed.bandNhood) && (is_inside_volume == true))		
					contributes_to_energy = true;
				else
					contributes_to_energy = false;

				if((abs(radial_dist - 1) < seed.bandNhood) && (is_inside_volume == false))		
					energy_band_outside_volume = true;
				else
					energy_band_outside_volume = false;

				if(contributes_to_energy == true){
					seed.xNormalizedInBand.push_back(x_normalized/radial_dist);
					seed.yNormalizedInBand.push_back(y_normalized/radial_dist);
					seed.zNormalizedInBand.push_back(z_normalized/radial_dist);
					seed.intensityInBand.push_back(data_ptr->GetPixel(current_index));
					seed.gxInBand.push_back(gx->GetPixel(current_index));
					seed.gyInBand.push_back(gy->GetPixel(current_index));
					seed.gzInBand.push_back(gz->GetPixel(current_index));

					seed.bandArea++;
				}

				if(energy_band_outside_volume == true){
					seed.xNormalizedInBand.push_back(x_normalized/radial_dist);
					seed.yNormalizedInBand.push_back(y_normalized/radial_dist);
					seed.zNormalizedInBand.push_back(z_normalized/radial_dist);
					seed.intensityInBand.push_back(0);
					seed.gxInBand.push_back(0);
					seed.gyInBand.push_back(0);
					seed.gzInBand.push_back(0);
					
					seed.bandArea++;
				}
				
				if(is_on_foreground == true){

					//std::cout << "Position: " << i << ", " << j << ", " << k << " Value: " << data_ptr->GetPixel(current_index) << std::endl;
					
					foreground_values.push_back(data_ptr->GetPixel(current_index));
					
					seed.meanForegroundIntensity = seed.meanForegroundIntensity + data_ptr->GetPixel(current_index);
					seed.foregroundArea++;
				}
				if(is_on_background == true){

					background_values.push_back(data_ptr->GetPixel(current_index));

					seed.meanBackgroundIntensity = seed.meanBackgroundIntensity + data_ptr->GetPixel(current_index);
					seed.backgroundArea++;
				}

			} 
		}
	}

	if(seed.isSecondary == true){
		
		if(foreground_values.size() != 0){
			// Calculate the median
			std::sort(foreground_values.begin(), foreground_values.end());

			seed.meanForegroundIntensity = *(foreground_values.begin() + foreground_values.size() / 2);
		}
		else
			seed.meanForegroundIntensity = 0.0;

		if(background_values.size() != 0){
			std::sort(background_values.begin(), background_values.end());
			seed.meanBackgroundIntensity = *(background_values.begin() + background_values.size() / 2);
		}
		else
			seed.meanBackgroundIntensity = 0.0;
	}
	else{

		// Calculate the mean
		seed.meanForegroundIntensity = seed.meanForegroundIntensity / (double)seed.foregroundArea;
		seed.meanBackgroundIntensity = seed.meanBackgroundIntensity / (double)seed.backgroundArea;
	}

	seed.likelihood = seed.meanForegroundIntensity - seed.meanBackgroundIntensity;

	// If likelood is less than 0.001, do what? According to the thesis, the subvolume should be discarded. But here, the likelihood value is
	// increased if it is less then 0.001
	/*double mean_of_means = 0.0;
	if(seed.likelihood < this->allParams.nodeDetectionParams.increaseLikelihoodThreshold){
		
		mean_of_means = (seed.meanForegroundIntensity + seed.meanBackgroundIntensity) / 2.0;
		seed.meanForegroundIntensity = mean_of_means + this->allParams.nodeDetectionParams.discardNodeLikelihoodThreshold;
		seed.meanBackgroundIntensity = mean_of_means - this->allParams.nodeDetectionParams.discardNodeLikelihoodThreshold;
		if(seed.meanBackgroundIntensity < 0)
			seed.meanBackgroundIntensity = 0;
		seed.likelihood = seed.meanForegroundIntensity - seed.meanBackgroundIntensity;
	}*/
	
	// Set seed as invalid if majority of its neighbourhood lies outside the data limits
	if(in_volume_count < out_volume_count)
		seed.isValid = false;
	else
		seed.isValid = true;	
}

void ftkVesselTracer::UpdateModel(Node& seed, int iter_number){

	double last_x = seed.x, last_y = seed.y, last_z = seed.z;
	double last_scale = seed.scale;

	std::vector<double> inside_region_term, outside_region_term;
	for(int i = 0; i < seed.intensityInBand.size(); i++){
		inside_region_term.push_back(std::abs(seed.meanForegroundIntensity - seed.intensityInBand[i]));
		outside_region_term.push_back(std::abs(PixelType(seed.meanBackgroundIntensity - seed.intensityInBand[i])));
	}
	
	std::vector<double> del_energy, region_based_term, gvf_based_term;
	//std::transform(inside_region_term.begin(), inside_region_term.end(), outside_region_term.begin(), del_energy.begin(), std::minus<double>());
	for(int i = 0; i < inside_region_term.size(); i++){
		region_based_term.push_back(inside_region_term[i] - outside_region_term[i]);
	}
	
	double region_weight = this->allParams.nodeDetectionParams.regionBasedTermWeight;
	double gvf_weight = this->allParams.nodeDetectionParams.edgeBasedTermWeight;
	if(iter_number < this->allParams.nodeDetectionParams.iterNForOnlyRegionBasedTerm)
		del_energy = region_based_term;
	else{
		for(int i = 0; i < seed.bandArea; i++)
			gvf_based_term.push_back(seed.xNormalizedInBand[i] * seed.gxInBand[i] + seed.yNormalizedInBand[i] * seed.gyInBand[i] + seed.zNormalizedInBand[i] * seed.gzInBand[i]);
		
		for(int i = 0; i < seed.bandArea; i++)
			del_energy.push_back(region_weight * region_based_term[i] - gvf_weight * gvf_based_term[i]);
	}
	
	double acc_energy = std::accumulate(del_energy.begin(), del_energy.end(), 0.0);
	double dscale = acc_energy / seed.bandArea;

	//std::cout << " Energy: " << acc_energy << std::endl;
	
	double dx = 0.0, dy = 0.0, dz = 0.0;
	for(int i = 0; i < seed.bandArea; i++){
		dx = dx + (seed.xNormalizedInBand[i] * del_energy[i]);
		dy = dy + (seed.yNormalizedInBand[i] * del_energy[i]);
		dz = dz + (seed.zNormalizedInBand[i] * del_energy[i]);
	}
	dx = dx / (double)seed.bandArea;
	dy = dy / (double)seed.bandArea;
	dz = dz / (double)seed.bandArea;
	
	dscale = dscale * this->allParams.nodeDetectionParams.dtScale;
	dx = dx * this->allParams.nodeDetectionParams.dtX;
	dy = dy * this->allParams.nodeDetectionParams.dtY;
	dz = dz * this->allParams.nodeDetectionParams.dtZ;

	dscale = std::min(1.0, std::max(-1.0, dscale));
	dx = std::min(1.0, std::max(-1.0, dx));
	dy = std::min(1.0, std::max(-1.0, dy));
	dz = std::min(1.0, std::max(-1.0, dz));

	seed.scale = seed.scale - dscale;
	seed.x = seed.x - dx;
	seed.y = seed.y - dy;
	seed.z = seed.z - dz;

	if(seed.scale > (double)this->allParams.nodeDetectionParams.maxVesselWidth)
		seed.scale = this->allParams.nodeDetectionParams.maxVesselWidth;
	if(seed.scale < (double)this->allParams.nodeDetectionParams.minVesselWidth)
		seed.scale = this->allParams.nodeDetectionParams.minVesselWidth;

	if(this->GetSign(dx) != this->GetSign((double)this->allParams.nodeDetectionParams.dirX))
		this->allParams.nodeDetectionParams.dtX = this->allParams.nodeDetectionParams.dtX * this->allParams.nodeDetectionParams.primaryReversePositionRate;
	if(this->GetSign(dy) != this->GetSign((double)this->allParams.nodeDetectionParams.dirY))
		this->allParams.nodeDetectionParams.dtY = this->allParams.nodeDetectionParams.dtY * this->allParams.nodeDetectionParams.primaryReversePositionRate;
	if(this->GetSign(dz) != this->GetSign((double)this->allParams.nodeDetectionParams.dirZ))
		this->allParams.nodeDetectionParams.dtZ = this->allParams.nodeDetectionParams.dtZ * this->allParams.nodeDetectionParams.primaryReversePositionRate;

	this->allParams.nodeDetectionParams.dirX = this->GetSign(dx);
	this->allParams.nodeDetectionParams.dirY = this->GetSign(dy);
	this->allParams.nodeDetectionParams.dirZ = this->GetSign(dz);

	this->allParams.nodeDetectionParams.chX[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.x - last_x;
	this->allParams.nodeDetectionParams.chY[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.y - last_y;
	this->allParams.nodeDetectionParams.chZ[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.z - last_z;

	
	if((dscale * (double)this->allParams.nodeDetectionParams.dirScale) <= 0.0)
		this->allParams.nodeDetectionParams.dtScale = this->allParams.nodeDetectionParams.dtScale * this->allParams.nodeDetectionParams.primaryReverseScaleRate;

	this->allParams.nodeDetectionParams.dirScale = this->GetSign(dscale);
	this->allParams.nodeDetectionParams.chScale[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.scale - last_scale;
}

void ftkVesselTracer::UpdateModelSecondary(Node& seed, Node& anchor_node, int iter_number){

	double last_x = seed.x, last_y = seed.y, last_z = seed.z;
	double last_scale = seed.scale;
	
	std::vector<double> inside_region_term, outside_region_term;
	for(int i = 0; i < seed.intensityInBand.size(); i++){
		inside_region_term.push_back(std::abs(seed.meanForegroundIntensity - seed.intensityInBand[i]));
		outside_region_term.push_back(std::abs(PixelType(seed.meanBackgroundIntensity - seed.intensityInBand[i])));
	}
	
	std::vector<double> del_energy, region_based_term, gvf_based_term;
	//std::transform(inside_region_term.begin(), inside_region_term.end(), outside_region_term.begin(), del_energy.begin(), std::minus<double>());
	for(int i = 0; i < inside_region_term.size(); i++){
		region_based_term.push_back(inside_region_term[i] - outside_region_term[i]);
	}
	
	double region_weight = this->allParams.nodeDetectionParams.regionBasedTermWeightSecondary;
	double gvf_weight = this->allParams.nodeDetectionParams.edgeBasedTermWeightSecondary;
	if(iter_number < this->allParams.nodeDetectionParams.iterNForOnlyRegionBasedTerm)
		del_energy = region_based_term;
	else{
		for(int i = 0; i < seed.bandArea; i++)
			gvf_based_term.push_back(seed.xNormalizedInBand[i] * seed.gxInBand[i] + seed.yNormalizedInBand[i] * seed.gyInBand[i] + seed.zNormalizedInBand[i] * seed.gzInBand[i]);
		
		for(int i = 0; i < seed.bandArea; i++)
			del_energy.push_back(region_weight * region_based_term[i] - gvf_weight * gvf_based_term[i]);
	}

	double acc_energy = std::accumulate(del_energy.begin(), del_energy.end(), 0.0);
	double dscale = acc_energy / (double)seed.bandArea;

	//std::cout << " Energy: " << acc_energy << std::endl;
	
	double dx = 0.0, dy = 0.0, dz = 0.0, dc = 0.0;
	for(int i = 0; i < seed.bandArea; i++){
		dx = dx + (seed.xNormalizedInBand[i] * del_energy[i]);
		dy = dy + (seed.yNormalizedInBand[i] * del_energy[i]);
		dz = dz + (seed.zNormalizedInBand[i] * del_energy[i]);
	}
	dx = dx / (double)seed.bandArea;
	dy = dy / (double)seed.bandArea;
	dz = dz / (double)seed.bandArea;
	
	dscale = dscale * this->allParams.nodeDetectionParams.dtScale;
	dx = dx * this->allParams.nodeDetectionParams.dtX;
	dy = dy * this->allParams.nodeDetectionParams.dtY;
	dz = dz * this->allParams.nodeDetectionParams.dtZ;

	dscale = std::min(0.5, std::max(-0.5, dscale));
	dx = std::min(0.5, std::max(-0.5, dx));
	dy = std::min(0.5, std::max(-0.5, dy));
	dz = std::min(0.5, std::max(-0.5, dz));

	dc = (anchor_node.dirX[0]*dx) + (anchor_node.dirY[0]*dy) + (anchor_node.dirZ[0]*dz);

	seed.scale = seed.scale - dscale;
	seed.x = seed.x - dx + dc*anchor_node.dirX[0];
	seed.y = seed.y - dy + dc*anchor_node.dirY[0];
	seed.z = seed.z - dz + dc*anchor_node.dirZ[0];

	if(seed.scale > (double)this->allParams.nodeDetectionParams.maxVesselWidth)
		seed.scale = this->allParams.nodeDetectionParams.maxVesselWidth;
	if(seed.scale < (double)this->allParams.nodeDetectionParams.minVesselWidth)
		seed.scale = this->allParams.nodeDetectionParams.minVesselWidth;

	std::vector<double> ch_dir(3, 0.0), ch_dir_normalized(3, 0.0);
	ch_dir[0] = seed.x - anchor_node.xInitSecondary;
	ch_dir[1] = seed.y - anchor_node.yInitSecondary;
	ch_dir[2] = seed.z - anchor_node.zInitSecondary;
	double ch_norm = Node::ComputeNorm(Node(ch_dir[0], ch_dir[1], ch_dir[2], 0));
	ch_dir_normalized[0] = ch_dir[0]/ch_norm;
	ch_dir_normalized[1] = ch_dir[1]/ch_norm;
	ch_dir_normalized[2] = ch_dir[2]/ch_norm;
	
	// Restrict the search space for secondary nodes
	if(ch_norm > this->allParams.nodeDetectionParams.secondarySearchConstraint * seed.scale){	
		seed.x = anchor_node.xInitSecondary + ch_dir[0] * (this->allParams.nodeDetectionParams.secondarySearchConstraint * seed.scale / ch_norm);
		seed.y = anchor_node.yInitSecondary + ch_dir[1] * (this->allParams.nodeDetectionParams.secondarySearchConstraint * seed.scale / ch_norm);
		seed.z = anchor_node.zInitSecondary + ch_dir[2] * (this->allParams.nodeDetectionParams.secondarySearchConstraint * seed.scale / ch_norm);
	}

	if(this->GetSign(dx) != this->GetSign(this->allParams.nodeDetectionParams.dirX))
		this->allParams.nodeDetectionParams.dtX = this->allParams.nodeDetectionParams.dtX * this->allParams.nodeDetectionParams.secondaryReversePositionRate;
	if(this->GetSign(dy) != this->GetSign(this->allParams.nodeDetectionParams.dirY))
		this->allParams.nodeDetectionParams.dtY = this->allParams.nodeDetectionParams.dtY * this->allParams.nodeDetectionParams.secondaryReversePositionRate;
	if(this->GetSign(dz) != this->GetSign(this->allParams.nodeDetectionParams.dirZ))
		this->allParams.nodeDetectionParams.dtZ = this->allParams.nodeDetectionParams.dtZ * this->allParams.nodeDetectionParams.secondaryReversePositionRate;

	this->allParams.nodeDetectionParams.dirX = this->GetSign(dx);
	this->allParams.nodeDetectionParams.dirY = this->GetSign(dy);
	this->allParams.nodeDetectionParams.dirZ = this->GetSign(dz);

	this->allParams.nodeDetectionParams.chX[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.x - last_x;
	this->allParams.nodeDetectionParams.chY[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.y - last_y;
	this->allParams.nodeDetectionParams.chZ[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.z - last_z;

	
	if((dscale * (double)this->allParams.nodeDetectionParams.dirScale) <= 0.0)
		this->allParams.nodeDetectionParams.dtScale = this->allParams.nodeDetectionParams.dtScale * this->allParams.nodeDetectionParams.secondaryReverseScaleRate;

	this->allParams.nodeDetectionParams.dirScale = this->GetSign(dscale); // = dscale; // As given in Matlab code
	this->allParams.nodeDetectionParams.chScale[this->allParams.nodeDetectionParams.currentMonitoredIter] = seed.scale - last_scale;
}

int inline ftkVesselTracer::GetSign(double value){
	if(value + 0.001 < 0)
		return -1;
	if(value - 0.001 > 0) 
		return 1;
	//std::cout << "ye";
	return 0;
}

bool ftkVesselTracer::ExitModelFitting(Node& seed, int iter_number){
	
	bool exit_fitting = false;
	this->allParams.nodeDetectionParams.currentMonitoredIter = (iter_number % this->allParams.nodeDetectionParams.iterNMonitorParamChange); //+ 1;
	
	if(seed.scale >= (double)this->allParams.nodeDetectionParams.maxVesselWidth){
		seed.exitIter = iter_number;
		exit_fitting = true;
	}
	
	double total_ch_scale = 0.0, total_ch_x = 0.0, total_ch_y = 0.0, total_ch_z = 0.0, total_ch_position = 0.0;
	int iter_minimum = 0;
	if(seed.isSecondary == true)
		iter_minimum = this->allParams.nodeDetectionParams.iterNMinimumSecondary;
	else
		iter_minimum = this->allParams.nodeDetectionParams.iterNMinimum;

	if(iter_number > iter_minimum){
		for(int i = 0; i < this->allParams.nodeDetectionParams.iterNMonitorParamChange; i++){
			total_ch_scale = total_ch_scale + (double)std::abs(this->allParams.nodeDetectionParams.chScale[i]);
			total_ch_x = total_ch_x + (double)std::abs(this->allParams.nodeDetectionParams.chX[i]);
			total_ch_y = total_ch_y + (double)std::abs(this->allParams.nodeDetectionParams.chY[i]);
			total_ch_z = total_ch_z + (double)std::abs(this->allParams.nodeDetectionParams.chZ[i]);
		}
		total_ch_position = total_ch_x + total_ch_y + total_ch_z;

		if(total_ch_scale < this->allParams.nodeDetectionParams.minimumAccumulatedParamChange && total_ch_position < this->allParams.nodeDetectionParams.minimumAccumulatedParamChange){
			seed.exitIter = iter_number;
			exit_fitting = true;
		}
	}
	if(iter_number == this->allParams.nodeDetectionParams.iterNPrimaryNode)
		seed.exitIter = iter_number;
	
	return exit_fitting;
}

void ftkVesselTracer::SortAndFilterPrimaryNodes(void){

	// Remove nodes lying outside the image space
	ImageType3D::IndexType index;
	for(int i = 0; i < this->primaryNodes.size(); i++){
		
		index[0] = this->primaryNodes[i].x; index[1] = this->primaryNodes[i].y; index[2] = this->primaryNodes[i].z;

		if(this->normalizedInputData->GetLargestPossibleRegion().IsInside(index) == false){
			this->primaryNodes.erase(this->primaryNodes.begin() + i);
			std::cout << "Primary node out-of-bounds erased. " << std::endl;
		}
	}

	// Sort the primary nodes in descending order of likelihood
	std::sort(this->primaryNodes.begin(), this->primaryNodes.end(), compareNodesByLikelihood);
	std::reverse(this->primaryNodes.begin(), this->primaryNodes.end());

	//for(int i = 0; i < 100; i++)
	//	std::cout << i << ": " << this->primaryNodes[i].likelihood << ", " << this->primaryNodes[i].scale << std::endl;

	// Remove nodes which are very close to each other (node with higher likelihood remains)
	this->primaryNodesAfterHitTest.push_back(this->primaryNodes[0]);
	double norm = 0.0;
	bool hit = false;
	
	for(int i = 1; i < this->primaryNodes.size(); i++){		
		
		Node current_primary_node = this->primaryNodes[i];
		hit = false;
		for(int j = 0; j < this->primaryNodesAfterHitTest.size(); j++){
			
			Node current_primary_node_final = this->primaryNodesAfterHitTest[j];
			norm = Node::ComputeNorm(Node(current_primary_node.x - current_primary_node_final.x, current_primary_node.y - current_primary_node_final.y, 
				current_primary_node.z - current_primary_node_final.z, 0));
			if(norm < (this->allParams.nodeDetectionParams.distanceThresholdPrimary * (current_primary_node.scale + current_primary_node_final.scale))){
				//std::cout << "Hit! norm: " << norm << " Sum of scales: " << (current_primary_node.scale + current_primary_node_final.scale) << std::endl;
				hit = true;
				break;
			}
		}
		if(hit == false)
			this->primaryNodesAfterHitTest.push_back(current_primary_node);
	}

	//std::cout << "Final primary nodes: " << this->primaryNodesAfterHitTest.size() << std::endl;
	//for(int i = 0; i < this->primaryNodesAfterHitTest.size(); i++)
	//	std::cout << " Scale: " << this->primaryNodesAfterHitTest[i].scale << " Likelihood: " << this->primaryNodesAfterHitTest[i].likelihood << std::endl;
}

void ftkVesselTracer::ComputeAllSecondaryNodes(void){

	//// For testing purposes
	//ImageType3D::IndexType test_index, test_index1;
	//test_index[0] = 128; test_index[1] = 7; test_index[2] = 25;
	//test_index1[0] = 6; test_index1[1] = 127; test_index1[2] = 24;
	//PixelType test_pixel = this->inputData->GetPixel(test_index);
	//PixelType test_pixel1 = this->inputData->GetPixel(test_index1);

	////this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());

	//ImageType3D::IndexType test_index2;
	//test_index2[0] = this->primaryNodesAfterHitTest[0].x; 
	//test_index2[1] = this->primaryNodesAfterHitTest[0].y;
	//test_index2[2] = this->primaryNodesAfterHitTest[0].z;
	//PixelType test_pixel2 = this->inputData->GetPixel(test_index2);
	//PixelType test_pixel3 = this->normalizedInputData->GetPixel(test_index2);
	
	
	clock_t start_tracing_time, stop_tracing_time;
	double tracing_time = 0.0;

	assert((start_tracing_time = clock()) != -1);

	std::cout << "Started with tracing... " << std::endl;
	
	// For testing
	//Node a_node;
	//a_node.scale = 2.5;
	//a_node.y = 57; //124.7428;
	//a_node.x = 161; //64.4287;
	//a_node.z = 25;
	//a_node.likelihood = 0.4169;
	//a_node.meanForegroundIntensity = 0.5671;
	//a_node.meanBackgroundIntensity = 0.1502;
	//a_node.parentIDLength = 4;
	//this->allParams.nodeDetectionParams.traceLengthCost = 0.5;
	//a_node.nHoodSecondaryMultiplier = 2;
	//this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());
	//this->primaryNodesAfterHitTest[0] = a_node; 
	//this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());
	
	//this->VisualizeNodesWithData3D(this->primaryNodesAfterHitTest, false);

	//PriorityQueueType node_queue(compareNodes(2));
	std::vector<queue_element> light_node_queue(this->allParams.nodeDetectionParams.maxQueueSize, queue_element(0, this->allParams.nodeDetectionParams.infTraceQuality));
	std::vector<double> quality_array;	

	//Node a_node;
	double trace_quality = 0.0;
	for(int i = 0; i < this->primaryNodesAfterHitTest.size(); i++){
		//Node a_node = this->primaryNodesAfterHitTest[i];           

		// Quality estimate for single nodes. Nodes with high likelihood get low quality, nodes with low likelihood get high quality
		if(this->primaryNodesAfterHitTest[i].likelihood <= 0){
			this->primaryNodesAfterHitTest[i].traceQuality = this->allParams.nodeDetectionParams.maxTraceCost; //.traceQualityThreshold;
			quality_array.push_back(this->allParams.nodeDetectionParams.maxTraceCost);
			light_node_queue[i] = queue_element(i, this->allParams.nodeDetectionParams.maxTraceCost);
		}
		else{
			trace_quality = -1.0 * std::log(this->primaryNodesAfterHitTest[i].likelihood) + this->allParams.nodeDetectionParams.traceLengthCost;
			this->primaryNodesAfterHitTest[i].traceQuality = trace_quality;
			quality_array.push_back(trace_quality);
			light_node_queue[i] = queue_element(i, trace_quality);
		}
	
		this->primaryNodesAfterHitTest[i].parentID = std::vector<double>(this->primaryNodesAfterHitTest[i].parentIDLength, -1);
		//node_queue.push(this->primaryNodesAfterHitTest[i]);
	}

	//std::cout << "Primary node trace quality.. " << std::endl;
	//for(int i = 0; i < quality_array.size(); i++)
	//	std::cout << quality_array[i] << std::endl;
	
	int total_nodes_counter = -1, hit = 0, primary_counter = 0, hit_counter = 0, queue_iter = -1;
	int queue_size = this->primaryNodesAfterHitTest.size();
	Node current_node, dir_node;
	double dirX = 0.0, dirY = 0.0, dirZ = 0.0, norm = 0.0;
	std::vector<double> dir_hist; 
	std::vector<Node> badNodes, veryBadNodes;
	queue_element current_queue_element(0, 0.0);
	
	//while(queue_iter <= this->primaryNodesAfterHitTest.size() && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter && !node_queue.empty()){
	//while(!node_queue.empty() && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter){
	while(queue_iter <= queue_size && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter){
		
		//current_node = node_queue.top();
		//node_queue.pop();
		
		this->GetBestTrace(light_node_queue, current_queue_element);

		// Break if queue is empty
		if(std::abs(current_queue_element.second - this->allParams.nodeDetectionParams.infTraceQuality) < 0.01)
			break;

		current_node = this->primaryNodesAfterHitTest[current_queue_element.first];

		if(current_node.parentID[0] == -1)
			current_node.isPrimary = true;
		else
			current_node.isPrimary = false;
	
		// If the quality of trace is above the threshold, these nodes have low quality (ignore them)
		if(current_queue_element.second > this->allParams.nodeDetectionParams.traceQualityThreshold && current_node.isPrimary == false){
		//if(current_node.traceQuality > this->allParams.nodeDetectionParams.traceQualityThreshold && current_node.isPrimary == false){
			//std::cout << current_queue_element.second << std::endl;
			continue;
		}
		
		hit = this->TraceHitTest(current_node);

		if(hit == 2 || (hit == 1 && current_node.isPrimary == true))
			continue;

		if(current_node.isPrimary == true && hit == 0)
			primary_counter++;
		
		total_nodes_counter++;
		this->allNodes.push_back(current_node);

		//std::cout << total_nodes_counter << " " << this->allNodes.size() << std::endl;

		if(hit == 1){

			// These will be secondary nodes only
			hit_counter++;

			dir_hist = current_node.sphHistRegionBased;
			this->allNodes[total_nodes_counter].sphHistRegionBased.clear();
			this->ComputeSecondaryNodeDirections(current_node, dir_hist);

			if(!current_node.dirX.empty() && !current_node.dirY.empty() && !current_node.dirZ.empty()){
				this->allNodes[total_nodes_counter].dirX = current_node.dirX;
				this->allNodes[total_nodes_counter].dirY = current_node.dirY;
				this->allNodes[total_nodes_counter].dirZ = current_node.dirZ;
			}
			else{
				// if nowhere to go, all there dirs are zero
				this->allNodes[total_nodes_counter].dirX.push_back(0.0);
				this->allNodes[total_nodes_counter].dirY.push_back(0.0);
				this->allNodes[total_nodes_counter].dirZ.push_back(0.0);
			}
			
			continue;
		}
		
		int p = current_node.parentID[0];
		//if(current_node.isPrimary == true){
		if(p == -1){	
			dir_hist = std::vector<double>(this->allParams.oriBin.angleCount, 1.0/(double)this->allParams.oriBin.angleCount);
			current_node.dirX.push_back(0.0);
			current_node.dirY.push_back(0.0);
			current_node.dirZ.push_back(0.0);
		}
		else{		
			dirX = this->allNodes[current_node.parentID[0]].x - current_node.x;
			dirY = this->allNodes[current_node.parentID[0]].y - current_node.y;
			dirZ = this->allNodes[current_node.parentID[0]].z - current_node.z;
			
			dir_node = Node(dirX, dirY, dirZ, 0);
			
			norm = Node::ComputeNorm(dir_node);
			dir_node = Node(dirX/norm, dirY/norm, dirZ/norm, 0);
			dir_hist = current_node.sphHistRegionBased;

			this->allNodes[total_nodes_counter].sphHistRegionBased.clear();
		}

		this->ComputeSecondaryNodeDirections(current_node, dir_hist);
		
		if(!current_node.dirX.empty() && !current_node.dirY.empty() && !current_node.dirZ.empty()){
			this->allNodes[total_nodes_counter].dirX = current_node.dirX;
			this->allNodes[total_nodes_counter].dirY = current_node.dirY;
			this->allNodes[total_nodes_counter].dirZ = current_node.dirZ;
		}
		else{

			//std::cout << "Empty dir: " << total_nodes_counter << std::endl;

			// if nowhere to go, all there dirs are zero
			this->allNodes[total_nodes_counter].dirX.push_back(0.0);
			this->allNodes[total_nodes_counter].dirY.push_back(0.0);
			this->allNodes[total_nodes_counter].dirZ.push_back(0.0);
		}

		if(current_node.dirX.empty() || current_node.dirY.empty() || current_node.dirZ.empty()){
			//std::cout << "Empty dir: " << total_nodes_counter << std::endl;
			continue;
		}

		std::vector<double> current_dir(3, 0.0);
		//Node secondary_node;
		for(int i = 0; i < current_node.dirX.size(); i++){
			
			// Only branches less than maxBranchAngle are considered
			if(((current_node.dirX[i]*dirX) + (current_node.dirY[i]*dirY) + (current_node.dirZ[i]*dirZ)) > std::cos(this->allParams.nodeDetectionParams.maxBranchAngle*vnl_math::pi/180.0)){
				//std::cout << std::cos(this->allParams.nodeDetectionParams.maxBranchAngle*vnl_math::pi/180.0) << std::endl;
				continue;
			}
			
			current_dir[0] = current_node.dirX[i];
			current_dir[1] = current_node.dirY[i];
			current_dir[2] = current_node.dirZ[i];
			
			Node secondary_node;
			secondary_node.parentIDLength = current_node.parentIDLength;
			secondary_node.parentID = std::vector<double>(current_node.parentIDLength, -1);

			this->FitSphereAtNodeSecondary(current_node, secondary_node, current_dir);			
			
			if(secondary_node.isValid == false){ // || secondary_node.likelihood <= 0){
				badNodes.push_back(secondary_node);
				quality_array.push_back(this->allParams.nodeDetectionParams.maxTraceCost);
				continue;
			}
			if(secondary_node.likelihood <= 0.0){
				veryBadNodes.push_back(secondary_node);
				quality_array.push_back(2.0 * this->allParams.nodeDetectionParams.maxTraceCost);
				//continue;
			}
			
			// Parent IDs of secondary node = circshift(parent IDs of current_node)
			secondary_node.parentID[0] = current_node.parentID.back(); //*(current_node.parentID.begin() + current_node.parentID.size()-1);
			for(int j = 1; j < current_node.parentID.size(); j++){
				secondary_node.parentID[j] = current_node.parentID[j-1];
			}
			secondary_node.parentID[0] = total_nodes_counter;
			
			//queue_size++;
			trace_quality = this->computeTraceQuality(secondary_node);

			this->primaryNodesAfterHitTest.push_back(secondary_node);
	
			//std::cout << "Trace quality: " << trace_quality << " Scale: " << secondary_node.scale << std::endl;
			
			//node_queue.push(current_node);
			//node_queue.push(secondary_node);

			quality_array.push_back(trace_quality);

			light_node_queue[queue_size] = queue_element(queue_size, trace_quality);
			queue_size++;
		}
		queue_iter++;
		
		//this->VisualizeNodesWithData3D(this->primaryNodesAfterHitTest, false);
		//this->VisualizeNodesWithData3D(this->allNodes, false);
	}

	stop_tracing_time = clock();
	tracing_time = (double)(stop_tracing_time - start_tracing_time)/CLOCKS_PER_SEC;

	std::cout << "Tracing took " << tracing_time << " seconds." << std::endl;
	std::cout << "Total nodes: " << this->allNodes.size() << std::endl;

	//Visualize all nodes
	//this->VisualizeNodesWithData3D(this->allNodes, true);
	this->VisualizeNodesWithData3D(this->allNodes, false);


	std::vector<Node> nodesWithNonPositiveLikelihood;
	for(int i = 0; i < this->allNodes.size(); i++){
		if(this->allNodes[i].likelihood <= 0.0)
			nodesWithNonPositiveLikelihood.push_back(this->allNodes[i]);
	}
	//this->VisualizeNodesWithData3D(nodesWithNonPositiveLikelihood, false);

	//if(badNodes.empty() == false)
	//	this->VisualizeNodesWithData3D(badNodes, false);

	//Write tracing result to image
	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->secondaryNodesImage = RenderImageType3D::New();
	this->secondaryNodesImage->SetRegions(id_reg);
	this->secondaryNodesImage->Allocate();
	this->secondaryNodesImage->SetSpacing(this->inputData->GetSpacing());

	this->secondaryNodesImage->FillBuffer(0);

	itk::Index<3> idx;
	for(int i = 0; i < this->allNodes.size(); i++){
		
		idx[0] = this->allNodes[i].x;
		idx[1] = this->allNodes[i].y;
		idx[2] = this->allNodes[i].z;

		this->secondaryNodesImage->SetPixel(idx, 255);
	}
	
	std::string secondary_nodes_file_name = this->data_folder_path;
	secondary_nodes_file_name.append("_SecondaryNodes.tif");

	ImageWriter::Pointer secondary_nodes_writer = ImageWriter::New();	
	secondary_nodes_writer->SetFileName(secondary_nodes_file_name);	
	secondary_nodes_writer->SetInput(this->secondaryNodesImage);
	secondary_nodes_writer->Update();


	//Write all nodes to a file
	//this->writeNodesToFile(this->allNodes, std::string("AllNodes.txt"));

	//write quality array
	/*ofstream nodes_file_stream;
	nodes_file_stream.open("QualityArray.txt", std::ios::out);

	if(nodes_file_stream.is_open() == true){
		for(int i = 0; i < quality_array.size(); i++)
			nodes_file_stream << quality_array[i] << std::endl;
		nodes_file_stream.close();
	}
	else
		std::cout << "File cannot be opened. " << std::endl;
		*/
}

void ftkVesselTracer::ComputeAllSecondaryNodes2(void){

	//// For testing purposes
	//ImageType3D::IndexType test_index, test_index1;
	//test_index[0] = 128; test_index[1] = 7; test_index[2] = 25;
	//test_index1[0] = 6; test_index1[1] = 127; test_index1[2] = 24;
	//PixelType test_pixel = this->inputData->GetPixel(test_index);
	//PixelType test_pixel1 = this->inputData->GetPixel(test_index1);

	////this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());

	//ImageType3D::IndexType test_index2;
	//test_index2[0] = this->primaryNodesAfterHitTest[0].x; 
	//test_index2[1] = this->primaryNodesAfterHitTest[0].y;
	//test_index2[2] = this->primaryNodesAfterHitTest[0].z;
	//PixelType test_pixel2 = this->inputData->GetPixel(test_index2);
	//PixelType test_pixel3 = this->normalizedInputData->GetPixel(test_index2);
	
	
	clock_t start_tracing_time, stop_tracing_time;
	double tracing_time = 0.0;

	assert((start_tracing_time = clock()) != -1);
	
	// For testing
	//Node a_node;
	//a_node.scale = 2.5;
	//a_node.y = 57; //124.7428;
	//a_node.x = 161; //64.4287;
	//a_node.z = 25;
	//a_node.likelihood = 0.4169;
	//a_node.meanForegroundIntensity = 0.5671;
	//a_node.meanBackgroundIntensity = 0.1502;
	//a_node.parentIDLength = 4;
	//this->allParams.nodeDetectionParams.traceLengthCost = 0.5;
	//a_node.nHoodSecondaryMultiplier = 2;
	//this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());
	//this->primaryNodesAfterHitTest[0] = a_node; 
	//this->primaryNodesAfterHitTest.erase(this->primaryNodesAfterHitTest.begin()+1, this->primaryNodesAfterHitTest.end());
	
	//this->VisualizeNodesWithData3D(this->primaryNodesAfterHitTest, false);

	PriorityQueueType node_queue(compareNodes(2));
	//std::vector<queue_element> light_node_queue(this->allParams.nodeDetectionParams.maxQueueSize, queue_element(0, this->allParams.nodeDetectionParams.infTraceQuality));
	std::vector<double> quality_array;	

	//Node a_node;
	double trace_quality = 0.0;
	for(int i = 0; i < this->primaryNodesAfterHitTest.size(); i++){
		//Node a_node = this->primaryNodesAfterHitTest[i];           

		// Quality estimate for single nodes. Nodes with high likelihood get low quality, nodes with low likelihood get high quality
		if(this->primaryNodesAfterHitTest[i].likelihood <= 0){
			this->primaryNodesAfterHitTest[i].traceQuality = this->allParams.nodeDetectionParams.maxTraceCost; //.traceQualityThreshold;
			quality_array.push_back(this->allParams.nodeDetectionParams.maxTraceCost);
			//light_node_queue[i] = queue_element(i, this->allParams.nodeDetectionParams.maxTraceCost);
		}
		else{
			trace_quality = -1.0 * std::log(this->primaryNodesAfterHitTest[i].likelihood) + this->allParams.nodeDetectionParams.traceLengthCost;
			this->primaryNodesAfterHitTest[i].traceQuality = trace_quality;
			quality_array.push_back(trace_quality);
			//light_node_queue[i] = queue_element(i, trace_quality);
		}
	
		this->primaryNodesAfterHitTest[i].parentID = std::vector<double>(this->primaryNodesAfterHitTest[i].parentIDLength, -1);
		node_queue.push(this->primaryNodesAfterHitTest[i]);
	}

	//std::cout << "Primary node trace quality.. " << std::endl;
	//for(int i = 0; i < quality_array.size(); i++)
	//	std::cout << quality_array[i] << std::endl;
	
	int total_nodes_counter = -1, hit = 0, primary_counter = 0, hit_counter = 0, queue_iter = -1;
	int queue_size = this->primaryNodesAfterHitTest.size();
	Node current_node, dir_node;
	double dirX = 0.0, dirY = 0.0, dirZ = 0.0, norm = 0.0;
	std::vector<double> dir_hist; 
	std::vector<Node> badNodes, veryBadNodes;
	queue_element current_queue_element(0, 0.0);
	
	//while(queue_iter <= this->primaryNodesAfterHitTest.size() && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter && !node_queue.empty()){
	while(!node_queue.empty() && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter){
	//while(queue_iter <= queue_size && queue_iter < this->allParams.nodeDetectionParams.maxQueueIter){
		
		current_node = node_queue.top();
		node_queue.pop();
		
		//this->GetBestTrace(light_node_queue, current_queue_element);

		// Break if queue is empty
		//if(std::abs(current_queue_element.second - this->allParams.nodeDetectionParams.infTraceQuality) < 0.01)
		//	break;

		//current_node = this->primaryNodesAfterHitTest[current_queue_element.first];

		if(current_node.parentID[0] == -1)
			current_node.isPrimary = true;
		else
			current_node.isPrimary = false;
	
		// If the quality of trace is above the threshold, these nodes have low quality (ignore them)
		//if(current_queue_element.second > this->allParams.nodeDetectionParams.traceQualityThreshold && current_node.isPrimary == false){
		if(current_node.traceQuality > this->allParams.nodeDetectionParams.traceQualityThreshold && current_node.isPrimary == false){
			//std::cout << current_queue_element.second << std::endl;
			continue;
		}
		
		hit = this->TraceHitTest(current_node);

		if(hit == 2 || (hit == 1 && current_node.isPrimary == true))
			continue;

		if(current_node.isPrimary == true && hit == 0)
			primary_counter++;
		
		total_nodes_counter++;
		this->allNodes.push_back(current_node);

		if(hit == 1){
			hit_counter++;
			continue;
		} 
		
		int p = current_node.parentID[0];
		//if(current_node.isPrimary == true){
		if(p == -1){	
			dir_hist = std::vector<double>(this->allParams.oriBin.angleCount, 1.0/(double)this->allParams.oriBin.angleCount);
			current_node.dirX.push_back(0.0);
			current_node.dirY.push_back(0.0);
			current_node.dirZ.push_back(0.0);
		}
		else{		
			dirX = this->allNodes[current_node.parentID[0]].x - current_node.x;
			dirY = this->allNodes[current_node.parentID[0]].y - current_node.y;
			dirZ = this->allNodes[current_node.parentID[0]].z - current_node.z;
			
			dir_node = Node(dirX, dirY, dirZ, 0);
			
			norm = Node::ComputeNorm(dir_node);
			dir_node = Node(dirX/norm, dirY/norm, dirZ/norm, 0);
			dir_hist = current_node.sphHistRegionBased;

			this->allNodes[total_nodes_counter].sphHistRegionBased.clear();
		}

		this->ComputeSecondaryNodeDirections(current_node, dir_hist);


		if(current_node.dirX.empty() || current_node.dirY.empty() || current_node.dirZ.empty())
			continue;

		std::vector<double> current_dir(3, 0.0);
		//Node secondary_node;
		for(int i = 0; i < current_node.dirX.size(); i++){
			
			// Only branches less than maxBranchAngle are considered
			if(((current_node.dirX[i]*dirX) + (current_node.dirY[i]*dirY) + (current_node.dirZ[i]*dirZ)) > std::cos(this->allParams.nodeDetectionParams.maxBranchAngle*vnl_math::pi/180.0)){
				//std::cout << std::cos(this->allParams.nodeDetectionParams.maxBranchAngle*vnl_math::pi/180.0) << std::endl;
				continue;
			}
			
			current_dir[0] = current_node.dirX[i];
			current_dir[1] = current_node.dirY[i];
			current_dir[2] = current_node.dirZ[i];
			
			Node secondary_node;
			secondary_node.parentIDLength = current_node.parentIDLength;
			secondary_node.parentID = std::vector<double>(current_node.parentIDLength, -1);

			this->FitSphereAtNodeSecondary(current_node, secondary_node, current_dir);			
			
			if(secondary_node.isValid == false){ // || secondary_node.likelihood <= 0){
				badNodes.push_back(secondary_node);
				quality_array.push_back(this->allParams.nodeDetectionParams.maxTraceCost);
				continue;
			}
			if(secondary_node.likelihood <= 0.0){
				veryBadNodes.push_back(secondary_node);
				quality_array.push_back(2.0 * this->allParams.nodeDetectionParams.maxTraceCost);
				//continue;
			}
			
			// Parent IDs of secondary node = circshift(parent IDs of current_node)
			secondary_node.parentID[0] = current_node.parentID.back(); //*(current_node.parentID.begin() + current_node.parentID.size()-1);
			for(int j = 1; j < current_node.parentID.size(); j++){
				secondary_node.parentID[j] = current_node.parentID[j-1];
			}
			secondary_node.parentID[0] = total_nodes_counter;
			
			//queue_size++;
			this->primaryNodesAfterHitTest.push_back(secondary_node);

			trace_quality = this->computeTraceQuality(secondary_node);
	
			std::cout << "Trace quality: " << trace_quality << " Scale: " << secondary_node.scale << std::endl;
			
			//node_queue.push(current_node);
			node_queue.push(secondary_node);

			quality_array.push_back(trace_quality);

			//light_node_queue[queue_size] = queue_element(queue_size, trace_quality);
			queue_size++;
		}
		queue_iter++;
		
		//this->VisualizeNodesWithData3D(this->primaryNodesAfterHitTest, false);
		//this->VisualizeNodesWithData3D(this->allNodes, false);
	}

	stop_tracing_time = clock();
	tracing_time = (double)(stop_tracing_time - start_tracing_time)/CLOCKS_PER_SEC;

	std::cout << "Tracing took " << tracing_time << " seconds." << std::endl;
	std::cout << "Total nodes: " << this->allNodes.size() << std::endl;

	//Visualize all nodes
	//this->VisualizeNodesWithData3D(this->allNodes, true);
	this->VisualizeNodesWithData3D(this->allNodes, false);


	std::vector<Node> nodesWithNonPositiveLikelihood;
	for(int i = 0; i < this->allNodes.size(); i++){
		if(this->allNodes[i].likelihood <= 0.0)
			nodesWithNonPositiveLikelihood.push_back(this->allNodes[i]);
	}
	//this->VisualizeNodesWithData3D(nodesWithNonPositiveLikelihood, false);

	//if(badNodes.empty() == false)
	//	this->VisualizeNodesWithData3D(badNodes, false);

	//Write tracing result to image
	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->secondaryNodesImage = RenderImageType3D::New();
	this->secondaryNodesImage->SetRegions(id_reg);
	this->secondaryNodesImage->Allocate();
	this->secondaryNodesImage->SetSpacing(this->inputData->GetSpacing());

	this->secondaryNodesImage->FillBuffer(0);

	itk::Index<3> idx;
	for(int i = 0; i < this->allNodes.size(); i++){
		
		idx[0] = this->allNodes[i].x;
		idx[1] = this->allNodes[i].y;
		idx[2] = this->allNodes[i].z;

		this->secondaryNodesImage->SetPixel(idx, 255);
	}
	
	std::string secondary_nodes_file_name = this->data_folder_path;
	secondary_nodes_file_name.append("_SecondaryNodes.tif");

	ImageWriter::Pointer secondary_nodes_writer = ImageWriter::New();	
	secondary_nodes_writer->SetFileName(secondary_nodes_file_name);	
	secondary_nodes_writer->SetInput(this->secondaryNodesImage);
	secondary_nodes_writer->Update();


	//Write all nodes to a file
	//this->writeNodesToFile(this->allNodes, std::string("AllNodes.txt"));

	//write quality array
	/*ofstream nodes_file_stream;
	nodes_file_stream.open("QualityArray.txt", std::ios::out);

	if(nodes_file_stream.is_open() == true){
		for(int i = 0; i < quality_array.size(); i++)
			nodes_file_stream << quality_array[i] << std::endl;
		nodes_file_stream.close();
	}
	else
		std::cout << "File cannot be opened. " << std::endl;
		*/
}

bool compareQueueElement(queue_element e1, queue_element e2){
	return(e1.second < e2.second);
}

void ftkVesselTracer::GetBestTrace(std::vector<queue_element>& queue, queue_element& top){

	std::sort(queue.begin(), queue.end(), compareQueueElement);
	top.first = queue[0].first;
	top.second = queue[0].second;

	queue[0].second = this->allParams.nodeDetectionParams.infTraceQuality;
}

int ftkVesselTracer::TraceHitTest(Node node){

	int hit = 0;

	ImageType3D::IndexType node_index;
	node_index[0] = node.x; node_index[1] = node.y; node_index[2] = node.z;
	if(this->normalizedInputData->GetLargestPossibleRegion().IsInside(node_index) == false){
		hit = 1;
		return hit;
	}
	
	double norm = 0.0;
	bool parent_node_hit = false, common_parents = false;
	std::vector<double> parent_intersection(2 * node.parentIDLength, 0);
	
	for(int i = 0; i < this->allNodes.size(); i++){		
		
		parent_node_hit = false;
		for(int j = 0; j < node.parentID.size(); j++){
			if(i == node.parentID[j]){
				parent_node_hit = true;
				break;
			}
		}
		if(parent_node_hit == true)
			continue;

		norm = Node::ComputeNorm(Node(this->allNodes[i].x - node.x, this->allNodes[i].y - node.y, this->allNodes[i].z - node.z, 0));
		if(norm < this->allParams.nodeDetectionParams.distanceThresholdSecondary * (this->allNodes[i].scale + node.scale)){

			parent_intersection = std::vector<double>(2 * node.parentIDLength, 0.0);
			//std::set_intersection(this->allNodes[i].parentID.begin(), this->allNodes[i].parentID.end(), node.parentID.begin(), node.parentID.end(), parent_intersection.begin());
			//std::set_intersection(this->allNodes[i].parentID.begin(), this->allNodes[i].parentID.begin()+this->allNodes[i].parentID.size(), node.parentID.begin(), node.parentID.begin()+node.parentID.size(), parent_intersection.begin());
			//std::set_intersection(this->allNodes[i].parentID, this->allNodes[i].parentID+this->allNodes[i].parentID.size(), node.parentID, node.parentID+node.parentID.size(), parent_intersection.begin());
			
			common_parents = false;
			for(int j = 0; j < this->allNodes[i].parentID.size(); j++){
				for(int k = 0; k < node.parentID.size(); k++){
					if(this->allNodes[i].parentID[j] == node.parentID[k]){
						common_parents = true;
						break;
					}
				}
			}

			/*common_parents = false;
			for(int k = 0; k < parent_intersection.size(); k++){
				if(k != 0){
					common_parents = true;
					break;
				}
			}*/

			if(common_parents == true)
				continue;
			
			hit = 1;
			//std::cout << hit << std::endl;
			if(norm < std::min(this->allNodes[i].scale, node.scale)){
				hit = 2;
				//std::cout << hit << std::endl;
			}

			return hit;
		}
	}
	return hit;
}

double ftkVesselTracer::computeTraceQuality(Node& node){

	double cost = 0.0; 
	if(node.likelihood <= 0.0)
		cost = this->allParams.nodeDetectionParams.traceQualityThreshold; //traceQualityThreshold; //.maxTraceCost;
	else
		cost = -1.0 * std::log(node.likelihood);

	int parent1 = node.parentID[0], parent2 = 0;
	std::vector<double> pos_diff(3, 0.0), pos_diff_last(3, 0.0);
	pos_diff[0] = this->allNodes[parent1].x - node.x;
	pos_diff[1] = this->allNodes[parent1].y - node.y;
	pos_diff[2] = this->allNodes[parent1].z - node.z;
	double pos_diff_norm = Node::ComputeNorm(Node(pos_diff[0], pos_diff[1], pos_diff[2], 0));
	pos_diff[0] = pos_diff[0] / pos_diff_norm;
	pos_diff[1] = pos_diff[1] / pos_diff_norm;
	pos_diff[2] = pos_diff[2] / pos_diff_norm;
	
	double current_curvature = 0.0, curr_vesselness, curr_likelihood;
	int count = 1;
	std::vector<double> arr_curvature, arr_vesselness, arr_likelihood;
	itk::Index<3> curr_idx, curr_idx1;
	double vesselness_diff;
	double vesselnessWt = this->allParams.nodeDetectionParams.vesselnessWeight;	
	for(int i = 1; i < node.parentID.size(); i++){
		parent2 = node.parentID[i];

		if(parent2 > -1){	
			pos_diff_last = pos_diff;
			pos_diff[0] = this->allNodes[parent2].x - this->allNodes[parent1].x;
			pos_diff[1] = this->allNodes[parent2].y - this->allNodes[parent1].y;
			pos_diff[2] = this->allNodes[parent2].z - this->allNodes[parent1].z;
			pos_diff_norm = Node::ComputeNorm(Node(pos_diff[0], pos_diff[1], pos_diff[2], 0));
			pos_diff[0] = pos_diff[0] / pos_diff_norm;
			pos_diff[1] = pos_diff[1] / pos_diff_norm;
			pos_diff[2] = pos_diff[2] / pos_diff_norm;
			
			current_curvature = (pos_diff[0] * pos_diff_last[0]) + (pos_diff[1] * pos_diff_last[1]) + (pos_diff[2] * pos_diff_last[2]);

			curr_idx[0] = this->allNodes[parent1].x; curr_idx[1] = this->allNodes[parent1].y; curr_idx[2] = this->allNodes[parent1].z;
			curr_idx1[0] = this->allNodes[parent2].x; curr_idx1[1] = this->allNodes[parent2].y; curr_idx1[2] = this->allNodes[parent2].z;
			
			curr_vesselness = this->VesselnessImage->GetPixel(curr_idx);
			curr_likelihood = this->allNodes[parent1].likelihood;
		}
		else{
			current_curvature = 1.0;
			curr_vesselness = 1.0;
			curr_likelihood = 1.0;
		}

		arr_curvature.push_back(current_curvature);
		arr_vesselness.push_back(curr_vesselness);
		arr_likelihood.push_back(curr_likelihood);
		

		// Cost function is different than one given in thesis?
		if(this->useVesselness){

			if(this->allNodes[parent1].likelihood <= 0.0 || curr_vesselness < this->allParams.nodeDetectionParams.vesselnessThershold)
				cost = this->allParams.nodeDetectionParams.traceQualityThreshold; //.maxTraceCost; //.traceQualityThreshold;
			else
				cost = cost - std::log(this->allNodes[parent1].likelihood + curr_vesselness); //- std::log(curr_vesselness);
		}
		else{
			if(this->allNodes[parent1].likelihood <= 0.0)
				cost = this->allParams.nodeDetectionParams.traceQualityThreshold; //.maxTraceCost; //.traceQualityThreshold;
			else
				cost = cost - std::log(this->allNodes[parent1].likelihood);
		}
		
		
		count = count + 1;

		if(parent2 == -1)
			break;
		parent1 = parent2;
	}

	double mean_curvature = std::accumulate(arr_curvature.begin(), arr_curvature.end(), 0.0);
	mean_curvature = mean_curvature / (double)arr_curvature.size();
	double std_curvature = 0.0;
	for(int i = 0; i < arr_curvature.size(); i++)
		std_curvature = std_curvature + std::pow(arr_curvature[i] - mean_curvature, 2);
	std_curvature = std::sqrt(std_curvature / (double)arr_curvature.size());

	double total_vesselness = std::accumulate(arr_vesselness.begin(), arr_vesselness.end(), 0.0);
	double total_likelihood = std::accumulate(arr_likelihood.begin(), arr_likelihood.end(), 0.0);
	
	if(this->useVesselness)
		node.traceQuality = vesselnessWt*(1.0/(total_vesselness + total_likelihood)) + std_curvature + ((cost + this->allParams.nodeDetectionParams.traceLengthCost)/(double)count);	
	else	
		node.traceQuality = std_curvature + ((cost + this->allParams.nodeDetectionParams.traceLengthCost)/(double)count);
	
	//node.traceQuality = 0.3*(1.0/(total_likelihood*total_vesselness)) + 0.4*std_curvature + 0.5*((cost + this->allParams.nodeDetectionParams.traceLengthCost)/(double)count);

	return node.traceQuality;
}

void Node::InitDefaultParamsBeforeODFRecursion(){
	this->nHoodSecondaryMultiplier = 2.0;
	this->nHoodScaleSecondary = this->nHoodSecondaryMultiplier * this->scale;
}

void ftkVesselTracer::ComputeSecondaryNodeDirections(Node& node, std::vector<double>& dir_hist){
	
	node.InitDefaultParamsBeforeODFRecursion();

	double nHoodScale = node.nHoodScaleSecondary;
	int sMaxBin = this->allParams.oriBin.indexLength;
	nHoodScale = std::min(nHoodScale, (double)sMaxBin);

	ImageType3D::IndexType nhood_start_index, nhood_end_index;
		
	nhood_start_index[0] = floor(node.x - nHoodScale + 0.5); 
	nhood_start_index[1] = floor(node.y - nHoodScale + 0.5);
	nhood_start_index[2] = floor(node.z - nHoodScale + 0.5);

	nhood_end_index[0] = floor(node.x + nHoodScale + 0.5);
	nhood_end_index[1] = floor(node.y + nHoodScale + 0.5);
	nhood_end_index[2] = floor(node.z + nHoodScale + 0.5);
	
	//ImageType3D::IndexType current_index, current_index_1;
	int in_volume_count = 0, out_volume_count = 0; //, hist_bin = 0, max_bin_element = 0, min_bin_element = 0;
	//PixelType x_normalized = 0, y_normalized = 0, z_normalized = 0, current_pixel = 0;
	//double radial_dist = 0, hist_val_region_based = 0;
	//std::vector<int> bin_index(3, 0);
	std::vector<double> sph_hist_region_based(this->allParams.oriBin.angleCount, 0.0);
	std::vector<double> sph_hist_count(this->allParams.oriBin.angleCount, 0.0);

	//#pragma omp parallel for
	for(int j = nhood_start_index[1]; j <= nhood_end_index[1]; j++){
		for(int i = nhood_start_index[0]; i <= nhood_end_index[0]; i++){
			for(int k = nhood_start_index[2]; k <= nhood_end_index[2]; k++){
			//for(int j = nhood_start_index[1]; j <= nhood_end_index[1]; j++){
				
				ImageType3D::IndexType current_index, current_index_1;
				current_index[0] = i; current_index[1] = j; current_index[2] = k;
				
				//current_index_1[0] = j; current_index_1[1] = i; current_index_1[2] = k;
				
				if(this->normalizedInputData->GetLargestPossibleRegion().IsInside(current_index) == true){
					in_volume_count++;
				
					PixelType x_normalized = ((double)i - node.x)/node.scale;
					PixelType y_normalized = ((double)j - node.y)/node.scale;
					PixelType z_normalized = ((double)k - node.z)/node.scale;
					double radial_dist = std::sqrt((x_normalized * x_normalized) + (y_normalized * y_normalized) + (z_normalized * z_normalized));

					
					if(radial_dist > 1.0 && radial_dist < node.nHoodSecondaryMultiplier	){
						
						std::vector<int> bin_index(3, 0);
						bin_index[0] = floor((double)i - node.x + 0.5); 
						bin_index[1] = floor((double)j - node.y + 0.5); 
						bin_index[2] = floor((double)k - node.z + 0.5);
						int max_bin_element = *std::max_element(bin_index.begin(), bin_index.end());
						int min_bin_element = *std::min_element(bin_index.begin(), bin_index.end());
						
						//#pragma omp critical
						if(max_bin_element < sMaxBin && min_bin_element > -1*sMaxBin){
							
							//int hist_bin1 = this->allParams.oriBin.binIndexVec[bin_index[0] + sMaxBin + 1][bin_index[1] + sMaxBin + 1][bin_index[2] + sMaxBin + 1];
							//int hist_bin4 = this->allParams.oriBin.binIndexVec[bin_index[0] + sMaxBin][bin_index[1] + sMaxBin][bin_index[2] + sMaxBin];

							//int hist_bin2 = this->allParams.oriBin.binIndexVec[bin_index[2] + sMaxBin][bin_index[1] + sMaxBin][bin_index[0] + sMaxBin];
							
							
							//int hist_bin = this->allParams.oriBin.binIndexVec[bin_index[2] + sMaxBin][bin_index[0] + sMaxBin][bin_index[1] + sMaxBin];
							int hist_bin = this->allParams.oriBin.binIndexVec[bin_index[2] + sMaxBin][bin_index[1] + sMaxBin][bin_index[0] + sMaxBin];
							
							
							//int hist_bin3 = this->allParams.oriBin.binIndexVec[bin_index[0] + sMaxBin][bin_index[1] + sMaxBin][bin_index[2] + sMaxBin];
							//hist_bin = this->allParams.oriBin.binIndexVec[bin_index[1] + sMaxBin][bin_index[0] + sMaxBin][bin_index[2] + sMaxBin];
							
							PixelType current_pixel = this->normalizedInputData->GetPixel(current_index);
							//float current_pixel_1 = this->normalizedInputData->GetPixel(current_index_1);
							
							//double hist_val_region_based  = current_pixel;
							double hist_val_region_based = std::abs(current_pixel - node.meanBackgroundIntensity) - std::abs(current_pixel - node.meanForegroundIntensity);
							
							sph_hist_region_based[hist_bin] = sph_hist_region_based[hist_bin] + hist_val_region_based;
							sph_hist_count[hist_bin]++;
						}
					}
				}
				else
					out_volume_count++;
			}
		}
	}

	// Print the histogram
	//for(int i = 0; i < sph_hist_region_based.size(); i++)
	//	std::cout << i << ", " << sph_hist_region_based[i] << std::endl;

	/*ofstream nodes_file_stream;
	nodes_file_stream.open("SphHist_Cpp.txt", std::ios::out);

	if(nodes_file_stream.is_open() == true){
		for(int i = 0; i < sph_hist_region_based.size(); i++)
			nodes_file_stream << sph_hist_region_based[i] << std::endl;
		nodes_file_stream.close();
	}*/

	//#pragma omp parallel for
	// Normalize sphrical histogram
	for(int i = 0; i < sph_hist_region_based.size(); i++){
		sph_hist_region_based[i] = sph_hist_region_based[i]/(sph_hist_count[i] + 0.001);		
		sph_hist_region_based[i] = std::max(this->allParams.oriBin.minSphHistCount, sph_hist_region_based[i]);
	}

	double hist_sum = std::accumulate(sph_hist_region_based.begin(), sph_hist_region_based.end(), 0.0);
	if(std::abs(hist_sum - 0.0) < 0.00001){
		node.dirX.clear(); node.dirY.clear(); node.dirZ.clear();
		node.sphHistRegionBased = sph_hist_region_based;
		return;
	}
	//#pragma omp parallel for
	for(int i = 0; i < sph_hist_region_based.size(); i++)
		sph_hist_region_based[i] = sph_hist_region_based[i]/hist_sum;

	// Obtain the smoothing prior - According to thesis, this should be a Gaussian transition prior
	std::vector<double> smooth_hist_1(this->allParams.oriBin.angleCount, 0.0), smooth_hist_2(this->allParams.oriBin.angleCount, 0.0);
	for(int i = 0; i < this->allParams.oriBin.angleCount; i++){
		smooth_hist_1[i] = dir_hist[i];
		for(int j = 0; j < this->allParams.oriBin.nLastIndicesOfInterest; j++)
			smooth_hist_1[i] = smooth_hist_1[i] + (this->allParams.oriBin.histSmoothingFactor * dir_hist[this->allParams.oriBin.nbr[i][j]]);
	}
	for(int i = 0; i < this->allParams.oriBin.angleCount; i++){
		//smooth_hist_2[i] = smooth_hist_1[i];
		dir_hist[i] = smooth_hist_1[i];
		for(int j = 0; j < this->allParams.oriBin.nLastIndicesOfInterest; j++){
			//smooth_hist_2[i] = smooth_hist_2[i] + (this->allParams.oriBin.histSmoothingFactor * smooth_hist_1[this->allParams.oriBin.nbr[i][j]]);
			dir_hist[i] = dir_hist[i] + (this->allParams.oriBin.histSmoothingFactor * smooth_hist_1[this->allParams.oriBin.nbr[i][j]]);
		}
	}
	
	//#pragma omp parallel for
	for(int i = 0; i < sph_hist_region_based.size(); i++){
		//sph_hist_region_based[i] = sph_hist_region_based[i] * smooth_hist_2[i];
		sph_hist_region_based[i] = sph_hist_region_based[i] * dir_hist[i];
	}
	
	hist_sum = std::accumulate(sph_hist_region_based.begin(), sph_hist_region_based.end(), 0.0);
	if(std::abs(hist_sum - 0.0) < 0.00001){
		node.dirX.clear(); node.dirY.clear(); node.dirZ.clear();
		node.sphHistRegionBased = sph_hist_region_based;
		return;
	}
	//#pragma omp parallel for
	for(int i = 0; i < sph_hist_region_based.size(); i++)
		sph_hist_region_based[i] = sph_hist_region_based[i]/hist_sum;

	// Print the histogram
	//for(int i = 0; i < sph_hist_region_based.size(); i++)
	//	std::cout << i << ", " << sph_hist_region_based[i] << std::endl;
	
	/*ofstream nodes_file_stream1;
	nodes_file_stream1.open("SphHist_Cpp_smoothed.txt", std::ios::out);

	if(nodes_file_stream1.is_open() == true){
		for(int i = 0; i < sph_hist_region_based.size(); i++)
			nodes_file_stream1 << sph_hist_region_based[i] << std::endl;
		nodes_file_stream1.close();
	}*/
	
	// Finding modes of the histogram
	bool mode_flag = false;
	std::vector<std::vector<double> > mode_bins;
	std::vector<double> mode_hist_val;
	std::vector<double> a_bin(3, 0.0);
	int index1 = 0, index2 = 0;
	// Find out the peaks of the histogram and the corresponding bin locations
	for(int i = 0; i < this->allParams.oriBin.angleCount; i++){
		mode_flag = true;
		if(std::abs(sph_hist_region_based[i] - 0.0) < 0.00001)
			continue;
		
		for(int j = 0; j < this->allParams.oriBin.nLastIndicesOfInterest; j++){
			
			//CHECK THE ACCESSING OF ALL 2D ARRAYS (COMPARED TO MATLAB)
			index1 = this->allParams.oriBin.nbr[i][j];
			index2 = i;
			
			//if(sph_hist_region_based[this->allParams.oriBin.nbr[i][j]] > sph_hist_region_based[i]){
			if(sph_hist_region_based[index1] > sph_hist_region_based[index2]){
				double v1 = sph_hist_region_based[index1];
				double v2 = sph_hist_region_based[index2];
				mode_flag = false;
				break;
			}
		}

		if(mode_flag == true){	
			a_bin[0] = this->allParams.oriBin.binCenters[0][i];
			a_bin[1] = this->allParams.oriBin.binCenters[1][i];
			a_bin[2] = this->allParams.oriBin.binCenters[2][i];
			mode_bins.push_back(a_bin);
			mode_hist_val.push_back(sph_hist_region_based[i]);
		}
	}
	
	// Refine node directions: get the top 2 peaks separated by 60 deg, see if the 3rd also higher than
	// 0.25*(p1 + p2)
	std::vector<std::vector<double> > mode_bins_sorted;
	std::vector<double> mode_hist_val_sorted;
	std::multimap<double, std::vector<double> > bin_and_val_map;
	std::multimap<double, std::vector<double> >::iterator iter_map;
	for(int i = 0; i < mode_bins.size(); i++)
		bin_and_val_map.insert(std::pair<double, std::vector<double> >(mode_hist_val[i], mode_bins[i]));
	
	for(iter_map = bin_and_val_map.begin(); iter_map != bin_and_val_map.end(); iter_map++){
		mode_hist_val_sorted.push_back((*iter_map).first);
		mode_bins_sorted.push_back((*iter_map).second);
	}
	std::reverse(mode_hist_val_sorted.begin(), mode_hist_val_sorted.end());
	std::reverse(mode_bins_sorted.begin(), mode_bins_sorted.end());
	
	double x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;
	std::vector<double> bin1(3, 0.0), bin2(3, 0.0);
	for(int i = 1; i < mode_bins_sorted.size(); i++){
		bin1 = mode_bins_sorted[i];
		for(int j = 0; j <= i - 1; j++){
			bin2 = mode_bins_sorted[j];
			if(((bin1[0]*bin2[0]) + (bin1[1]*bin2[1]) + (bin1[2]*bin2[2])) > std::cos(this->allParams.nodeDetectionParams.maxBranchAngle*vnl_math::pi/180.0))
				mode_hist_val_sorted[i] = 0.0;
		}
	}

	std::vector<std::vector<double> > mode_bins_sorted_1;
	std::vector<double> mode_hist_val_sorted_1;
	std::multimap<double, std::vector<double> > bin_and_val_map_1;
	std::multimap<double, std::vector<double> >::iterator iter_map_1, iter_map_2;
	for(int i = 0; i < mode_bins_sorted.size(); i++)
		bin_and_val_map_1.insert(std::pair<double, std::vector<double> >(mode_hist_val_sorted[i], mode_bins_sorted[i]));
	
	bin_and_val_map_1.erase(0.0);

	for(iter_map_1 = bin_and_val_map_1.begin(); iter_map_1 != bin_and_val_map_1.end(); iter_map_1++){

		//if(std::abs((*iter_map_1).first - 0.0) > 0.00001){
			mode_hist_val_sorted_1.push_back((*iter_map_1).first);
			mode_bins_sorted_1.push_back((*iter_map_1).second);
		//}
	}
	std::reverse(mode_hist_val_sorted_1.begin(), mode_hist_val_sorted_1.end());
	std::reverse(mode_bins_sorted_1.begin(), mode_bins_sorted_1.end());
	
	double branching_th = 0.0;
	std::vector<double> dirX, dirY, dirZ;
	if(mode_bins_sorted_1.size() > 2){
		branching_th = 0.5 * (mode_hist_val_sorted_1[0] + mode_hist_val_sorted_1[1]);

		if(mode_hist_val_sorted_1[2] > this->allParams.nodeDetectionParams.branchingThreshold * branching_th){
			for(int i = 0; i < 3; i++){
				dirX.push_back(mode_bins_sorted_1[i][0]);
				dirY.push_back(mode_bins_sorted_1[i][1]);
				dirZ.push_back(mode_bins_sorted_1[i][2]);
			}
		}
		else{
			for(int i = 0; i < 2; i++){
				dirX.push_back(mode_bins_sorted_1[i][0]);
				dirY.push_back(mode_bins_sorted_1[i][1]);
				dirZ.push_back(mode_bins_sorted_1[i][2]);
			}
		}
	}
	else{
		for(int i = 0; i < mode_bins_sorted_1.size(); i++){
			dirX.push_back(mode_bins_sorted_1[i][0]);
			dirY.push_back(mode_bins_sorted_1[i][1]);
			dirZ.push_back(mode_bins_sorted_1[i][2]);
		}
	}
	if(dirX.size() < 2){
		dirX.push_back(-1.0 * dirX[0]);
		dirY.push_back(-1.0 * dirY[0]);
		dirZ.push_back(-1.0 * dirZ[0]);
	}

	for(int i = 0; i < dirX.size(); i++){
		
		if(std::abs(dirX[i] - 0.0) < 0.000000001)
			dirX[i] = 0.0;	
		
		if(std::abs(dirY[i] - 0.0) < 0.000000001)
			dirY[i] = 0.0;
		
		if(std::abs(dirZ[i] - 0.0) < 0.000000001)
			dirZ[i] = 0.0;
	}

	node.dirX = dirX;
	node.dirY = dirY;
	node.dirZ = dirZ;
	node.sphHistRegionBased = sph_hist_region_based;

	// Print the detected maximal modes
	//for(int i = 0; i < node.dirX.size(); i++){
	//	std::cout << node.dirX[i] << ", " << node.dirY[i] << ", " << node.dirZ[i] << std::endl;
	//}
}

void ftkVesselTracer::writeNodesToFile(std::vector<Node>& nodes, std::string file_path){

	ofstream nodes_file_stream;
	nodes_file_stream.open(file_path.c_str(), std::ios::out);

	if(nodes_file_stream.is_open() == true){

		for(int i = 0; i < nodes.size(); i++){

			nodes_file_stream << nodes[i].x << "," << nodes[i].y << "," << nodes[i].z << "," << nodes[i].scale << "," << nodes[i].likelihood << "," 
				<< nodes[i].meanForegroundIntensity << "," << nodes[i].meanBackgroundIntensity << ",";
			for(int j = 0; j < nodes[i].parentID.size(); j++)
				nodes_file_stream << nodes[i].parentID[j] << ",";
			nodes_file_stream << std::endl;
		}
		nodes_file_stream.close();
	}
	else
		std::cout << "Unable to open file for writing nodes: " << file_path.c_str() << std::endl;
}

void ftkVesselTracer::ReadNodesFromTextFile(const std::string& file_name){

	if(this->allNodes.size() != 0)
		this->allNodes.clear();

	ifstream nodes_file_stream;
	nodes_file_stream.open(file_name.c_str(), std::ios::in);
	
	std::string str, str1;
	std::vector<std::string> node_str;
	if(nodes_file_stream.is_open() == true){

		while(nodes_file_stream.good() == true){
			
			std::string str;
			if(getline(nodes_file_stream, str) == false)
				break;
			//std::cout << str.c_str() << std::endl;

			std::istringstream str_stream(str); 
			while(str_stream.good() == true){
				
				if(getline(str_stream, str1, ',') == false)
					break;
				node_str.push_back(str1);
			}

			Node n1;
			n1.x = atof(node_str[0].c_str());
			n1.y = atof(node_str[1].c_str());
			n1.z = atof(node_str[2].c_str());
			n1.scale = atof(node_str[3].c_str());
			n1.likelihood = atof(node_str[4].c_str());
			n1.meanForegroundIntensity = atof(node_str[5].c_str());
			n1.meanBackgroundIntensity = atof(node_str[6].c_str());
			
			// Works only for parentID size = 4
			n1.parentIDLength = 4;
			n1.parentID = std::vector<double>(n1.parentIDLength, 0);
			n1.parentID[0] = atoi(node_str[7].c_str());
			n1.parentID[1] = atoi(node_str[8].c_str());
			n1.parentID[2] = atoi(node_str[9].c_str());
			n1.parentID[3] = atoi(node_str[10].c_str());
			
			this->allNodes.push_back(n1);	

			node_str.clear();
		}
		//if(!this->allNodes.empty())
			//this->VisualizeNodesWithData3D(this->allNodes, false);
	}
	else
		std::cout << "Unable to open the nodes file: " << file_name.c_str() << std::endl;
}

void Node::ComputeDistanceBetweenNodes(Node n1, Node n2, Node &nd){

	nd.x = n1.x - n2.x;
	nd.y = n1.y - n2.y;
	nd.z = n1.z - n2.z;
}

void Node::NormalizeNode(double norm_val){
	
	this->x = this->x / norm_val;
	this->y = this->y / norm_val;
	this->z = this->z / norm_val;
}

void Node::InvertNodeDir(void){

	this->x = -1 * this->x;
	this->y = -1 * this->y;
	this->z = -1 * this->z;
}

double Node::DotProduct(Node n1, Node n2){

	return(n1.x*n2.x + n1.y*n2.y + n1.z*n2.z);
}

AffinityEdge::AffinityEdge(void){

	this->from = 0;
	this->to = 0;
	this->weight = 0.0;
}

AffinityEdge::AffinityEdge(int from_node_index, int to_node_index, double edge_weight){

	this->from = from_node_index;
	this->to = to_node_index;
	this->weight = edge_weight;
}

bool CompareEdges(AffinityEdge e1, AffinityEdge e2){
	return(e1.weight > e2.weight);
}

Tree::Tree(void){

	this->ID = 0;
	this->start = 0;
	this->NNodes = 0;
}

Tree::Tree(int ID, int start, int NNodes){

	this->ID = ID;
	this->start = start;
	this->NNodes = NNodes;
}

void ftkVesselTracer::CreateMinimumSpanningForest(){
	
	this->CreateAffinityGraph();
	//this->ComputeMinimumSpanningTreeBoost();
	this->ComputeMinimumSpanningForestWithLoopDetection();
}

void ftkVesselTracer::CreateAffinityGraph(void){

	//Node dir;
	//double norm_dist = 0.0, existingNodeDist = 0.0;
	//int angleBinLinear = 0;

	std::cout << "Computing affinity graph.. " << std::endl;
	
	//allocate memory for the binned edges in every node
	for(int i = 0; i < this->allNodes.size(); i++)
		this->allNodes[i].connectedNodesBinned.resize(2 * this->allParams.graphAndMSTParams.NBinsAffinity, this->allParams.graphAndMSTParams.maxEdgeWeight+1);
	
	for(int i = 0; i < this->allNodes.size(); i++){
		
		#pragma omp parallel for
		for(int j = i + 1; j < this->allNodes.size(); j++){
			
			Node dir;
			Node::ComputeDistanceBetweenNodes(this->allNodes[j], this->allNodes[i], dir);
			double norm_dist = Node::ComputeNorm(dir);
			dir.NormalizeNode(norm_dist);

			if(norm_dist < this->allParams.graphAndMSTParams.affinityRadThresh * std::max(this->allNodes[j].scale, this->allNodes[i].scale)){
				
				int angleBinLinear = this->ComputeAffinityBin(dir);
				double existingNodeDist = this->allNodes[i].connectedNodesBinned[2*(angleBinLinear - 1) + 1];

				if(existingNodeDist > norm_dist){
					this->allNodes[i].connectedNodesBinned[2*(angleBinLinear - 1)] = j;
					this->allNodes[i].connectedNodesBinned[2*(angleBinLinear - 1) + 1] = norm_dist;
				}
				
				dir.InvertNodeDir();

				angleBinLinear = this->ComputeAffinityBin(dir);
				existingNodeDist = this->allNodes[j].connectedNodesBinned[2*(angleBinLinear - 1) + 1];
				
				if(existingNodeDist > norm_dist){
					this->allNodes[j].connectedNodesBinned[2*(angleBinLinear - 1)] = i;
					this->allNodes[j].connectedNodesBinned[2*(angleBinLinear - 1) + 1] = norm_dist;
				}				
			}
		}
	}
	
	Node dir1, dir;
	double norm_dist1 = 0.0, edge_weight = 0.0;
	double norm_dist = 0.0, existingNodeDist = 0.0;
	for(int i = 0; i < this->allNodes.size(); i++){
		for(int j = 0; j < 2*this->allParams.graphAndMSTParams.NBinsAffinity; j = j+2){
			if(this->allNodes[i].connectedNodesBinned[j+1] > this->allParams.graphAndMSTParams.maxEdgeWeight)
				continue;

			Node::ComputeDistanceBetweenNodes(this->allNodes[i], this->allNodes[this->allNodes[i].connectedNodesBinned[j]], dir);
			norm_dist = Node::ComputeNorm(dir);
			dir.NormalizeNode(norm_dist);

			for(int k = j+2; k < 2*this->allParams.graphAndMSTParams.NBinsAffinity; k = k+2){
				if(this->allNodes[i].connectedNodesBinned[k+1] > this->allParams.graphAndMSTParams.maxEdgeWeight)
					continue;
				
				Node::ComputeDistanceBetweenNodes(this->allNodes[i], this->allNodes[this->allNodes[i].connectedNodesBinned[k]], dir1);
				norm_dist1 = Node::ComputeNorm(dir1);
				dir1.NormalizeNode(norm_dist1);
				
				if(Node::DotProduct(dir, dir1) > cos(this->allParams.graphAndMSTParams.minBranchAngle)){
					if(this->allNodes[i].connectedNodesBinned[j+1] > this->allNodes[i].connectedNodesBinned[k+1])
						this->allNodes[i].connectedNodesBinned[j+1] = this->allParams.graphAndMSTParams.maxEdgeWeight + 1.0;
					else
						this->allNodes[i].connectedNodesBinned[k+1] = this->allParams.graphAndMSTParams.maxEdgeWeight + 1.0;
				}
			}
		}

		for(int j = 0; j < 2*this->allParams.graphAndMSTParams.NBinsAffinity; j = j+2){
			if(this->allNodes[i].connectedNodesBinned[j+1] < this->allParams.graphAndMSTParams.maxEdgeWeight + 1){
				
				// Edge weights are not similar to those mentioned in thesis
				// This edge weight is inverse of Amit's Matlab code - Higher weight would mean more likelihood of an edge being present
				
				Node n1 = this->allNodes[i], n2 = this->allNodes[this->allNodes[i].connectedNodesBinned[j]];

				//edge_weight = std::min(n1.likelihood, n2.likelihood)/this->allNodes[i].connectedNodesBinned[j+1];
				edge_weight = std::min(std::pow(n1.scale, 3) * n1.likelihood, std::pow(n2.scale, 3) * n2.likelihood) / this->allNodes[i].connectedNodesBinned[j+1];
				
				this->allNodes[i].connectedNodesAffinity.push_back(std::pair<int, double>(this->allNodes[i].connectedNodesBinned[j], edge_weight));

				this->edges.push_back(AffinityEdge(i, this->allNodes[i].connectedNodesBinned[j], edge_weight));
			}
		}
	}

	std::cout << "Done with computing affinity graph. " << std::endl;

	this->VisualizeAffinityGraph(true);



	//Toy graph
	/*this->edges.clear();
	this->edges.push_back(AffinityEdge(1, 7, 5));
	this->edges.push_back(AffinityEdge(7, 1, 5));
	this->edges.push_back(AffinityEdge(7, 6, 2));
	this->edges.push_back(AffinityEdge(6, 7, 2));
	this->edges.push_back(AffinityEdge(7, 4, 10));
	this->edges.push_back(AffinityEdge(4, 7, 10));
	this->edges.push_back(AffinityEdge(4, 6, 7));
	this->edges.push_back(AffinityEdge(6, 4, 7));
	this->edges.push_back(AffinityEdge(4, 5, 5));
	this->edges.push_back(AffinityEdge(5, 4, 5));
	this->edges.push_back(AffinityEdge(10, 2, 4));
	this->edges.push_back(AffinityEdge(2, 10, 4));
	this->edges.push_back(AffinityEdge(2, 3, 5));
	this->edges.push_back(AffinityEdge(3, 2, 5));
	this->edges.push_back(AffinityEdge(2, 9, 12));
	this->edges.push_back(AffinityEdge(9, 2, 12));
	this->edges.push_back(AffinityEdge(2, 8, 3));
	this->edges.push_back(AffinityEdge(8, 2, 3));
	this->edges.push_back(AffinityEdge(3, 9, 5));
	this->edges.push_back(AffinityEdge(9, 3, 5));
	this->edges.push_back(AffinityEdge(9, 8, 2));
	this->edges.push_back(AffinityEdge(8, 9, 2));
	this->edges.push_back(AffinityEdge(10, 4, 30));
	this->edges.push_back(AffinityEdge(4, 10, 30));*/


	

	// Sort the edges in descending order of weights (since higher weight is better)
	std::sort(this->edges.begin(), this->edges.end(), CompareEdges);

	// Initialize all forest nodes
	itk::Index<3> node_idx;
	this->allForestNodes = std::vector<Node>(this->allNodes.size());
	for(int i = 0; i < this->allNodes.size(); i++){
		this->allForestNodes[i].x = this->allNodes[i].x;
		this->allForestNodes[i].y = this->allNodes[i].y;
		this->allForestNodes[i].z = this->allNodes[i].z;
		this->allForestNodes[i].likelihood = this->allNodes[i].likelihood;
		this->allForestNodes[i].scale = this->allNodes[i].scale;

		node_idx[0] = this->allNodes[i].x;
		node_idx[1] = this->allNodes[i].y;
		node_idx[2] = this->allNodes[i].z;
		this->allForestNodes[i].vesselness = this->VesselnessImage->GetPixel(node_idx);

		this->allForestNodes[i].ID = this->allNodes[i].ID;
		this->allForestNodes[i].branchIDs = std::vector<int>(this->allParams.graphAndMSTParams.maxNBranches, -1);
		this->allForestNodes[i].NBranches = this->allNodes[i].NBranches;
		this->allForestNodes[i].traceQuality = this->allNodes[i].traceQuality;

		if(this->allNodes[i].dirX.empty() || this->allNodes[i].dirY.empty() || this->allNodes[i].dirZ.empty()){
			
			std::cout << "Empty dir!!! : " << i << std::endl;

			this->allForestNodes[i].dirX.push_back(0.0);
			this->allForestNodes[i].dirY.push_back(0.0);
			this->allForestNodes[i].dirZ.push_back(0.0);
		}
		else{
			this->allForestNodes[i].dirX = this->allNodes[i].dirX;
			this->allForestNodes[i].dirY = this->allNodes[i].dirY;
			this->allForestNodes[i].dirZ = this->allNodes[i].dirZ;
		}

	}

	// Used no longer
	//this->allNodes.clear();
}

int ftkVesselTracer::ComputeAffinityBin(Node dir){

	double phi = (atan2(dir.y, dir.x) / vnl_math::pi) + 1.0;
	double theta = acos(dir.z) / vnl_math::pi; 

	phi = floor(4.0*phi + 0.5);
	theta = floor(4.0*theta + 0.5);

	if(std::abs(phi - 0.0) < 0.0001)
		phi = 8.0;
	if(std::abs(theta - 0.0) < 0.0001)
		theta = 4.0;

	int bin = (int)(phi + 8.0*(theta - 1.0));
	
	return bin;
}

void ftkVesselTracer::VisualizeAffinityGraph(bool render_with_data = false){

	ITKToVTKConnectorType::Pointer ITK_to_VTK_connector = ITKToVTKConnectorType::New();

	ITK_to_VTK_connector->SetInput(this->inputDataForRendering);
	ITK_to_VTK_connector->Update();

	vtkSmartPointer<vtkImageData> vtk_image = ITK_to_VTK_connector->GetOutput();

	// Testing vtk image
	//vtk_image->PrintSelf(std::cout, vtkIndent(0));

	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderer->SetBackground(1.0, 1.0, 1.0);
	
	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();
	renderer->SetActiveCamera(camera);

	vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
	render_window->AddRenderer(renderer);
	
	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	render_window_interactor->SetRenderWindow(render_window);
	
	vtkSmartPointer<vtkPiecewiseFunction> opacity_transfer_function = vtkSmartPointer<vtkPiecewiseFunction>::New();
	opacity_transfer_function->AddPoint(2, 0.0);
	opacity_transfer_function->AddPoint(10, 0.1);
	
	vtkSmartPointer<vtkColorTransferFunction> color_transfer_function = vtkSmartPointer<vtkColorTransferFunction>::New();
	color_transfer_function->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
	color_transfer_function->AddRGBPoint((10 * 255)/100, 1, 1, 1); //blue
	color_transfer_function->AddRGBPoint((45 * 255)/100, 0, .01, 0); //green
	color_transfer_function->AddRGBPoint((150 * 255)/100, .01, 0, 0); //red
	
	vtkSmartPointer<vtkVolumeProperty> volume_property = vtkSmartPointer<vtkVolumeProperty>::New();
	volume_property->SetColor(color_transfer_function);
	volume_property->SetScalarOpacity(opacity_transfer_function);
	volume_property->ShadeOn();
	volume_property->SetInterpolationTypeToLinear();
	
	vtkSmartPointer<vtkFixedPointVolumeRayCastMapper> volume_mapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
	
	#if VTK_MAJOR_VERSION <= 5
		volume_mapper->SetInput(vtk_image);
	#else
		volume_mapper->SetInputData(vtk_image);
	#endif
	volume_mapper->SetBlendModeToComposite();
	
	vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
	volume->SetMapper(volume_mapper); 
	volume->SetProperty(volume_property);
	volume->SetPosition(0, 0, 0);
	volume->SetPickable(0);
	volume->Update();


	// Preparing vtkPolyData to show the graph
	
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	
	vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
	
	int point_counter = -1;
	for(int i = 0; i < this->allNodes.size(); i++){
		points->InsertNextPoint(this->allNodes[i].x, this->allNodes[i].y, this->allNodes[i].z);
		point_counter++;

		for(int j = 0; j < this->allNodes[i].connectedNodesAffinity.size(); j++){
			points->InsertNextPoint(this->allNodes[this->allNodes[i].connectedNodesAffinity[j].first].x, 
				this->allNodes[this->allNodes[i].connectedNodesAffinity[j].first].y, this->allNodes[this->allNodes[i].connectedNodesAffinity[j].first].z);			
			point_counter++;
		}
		for(int j = 0; j < this->allNodes[i].connectedNodesAffinity.size(); j++){
			vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
			line->GetPointIds()->SetId(0, point_counter - this->allNodes[i].connectedNodesAffinity.size());
			line->GetPointIds()->SetId(1, point_counter - j);
			lines->InsertNextCell(line);
		}
	}
	
	unsigned char green[3] = {0, 255, 0};
	vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
	colors->SetNumberOfComponents(3);
	colors->InsertNextTupleValue(green);
	
	vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
	poly_data->SetPoints(points);
	poly_data->SetLines(lines);
	//poly_data->GetCellData()->SetScalars(colors);
	
	vtkSmartPointer<vtkPolyDataMapper> poly_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	#if VTK_MAJOR_VERSION <= 5
		poly_mapper->SetInput(poly_data);
	#else
		poly_mapper->SetInputData(poly_data);
	#endif

	vtkSmartPointer<vtkActor> poly_actor = vtkSmartPointer<vtkActor>::New();
	poly_actor->SetMapper(poly_mapper);
	poly_actor->GetProperty()->SetLineWidth(3);
	poly_actor->GetProperty()->SetColor(0, 255, 0);
	
	renderer->AddActor(poly_actor);

	if(render_with_data)
		renderer->AddVolume(volume);

	renderer->ResetCamera();
	
	render_window_interactor->Initialize();
	render_window->Render();
	render_window_interactor->Start();
}

void ftkVesselTracer::VisualizeMinimumSpanningForest(bool render_with_data = false){
	
	ITKToVTKConnectorType::Pointer ITK_to_VTK_connector = ITKToVTKConnectorType::New();

	ITK_to_VTK_connector->SetInput(this->inputDataForRendering);
	ITK_to_VTK_connector->Update();

	vtkSmartPointer<vtkImageData> vtk_image = ITK_to_VTK_connector->GetOutput();

	// Testing vtk image
	//vtk_image->PrintSelf(std::cout, vtkIndent(0));

	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderer->SetBackground(1.0, 1.0, 1.0);
	
	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();
	renderer->SetActiveCamera(camera);

	vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
	render_window->AddRenderer(renderer);
	
	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	render_window_interactor->SetRenderWindow(render_window);
	
	vtkSmartPointer<vtkPiecewiseFunction> opacity_transfer_function = vtkSmartPointer<vtkPiecewiseFunction>::New();
	opacity_transfer_function->AddPoint(2, 0.0);
	opacity_transfer_function->AddPoint(10, 0.1);
	
	vtkSmartPointer<vtkColorTransferFunction> color_transfer_function = vtkSmartPointer<vtkColorTransferFunction>::New();
	color_transfer_function->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
	color_transfer_function->AddRGBPoint((10 * 255)/100, 1, 1, 1); //blue
	color_transfer_function->AddRGBPoint((45 * 255)/100, 0, .01, 0); //green
	color_transfer_function->AddRGBPoint((150 * 255)/100, .01, 0, 0); //red
	
	vtkSmartPointer<vtkVolumeProperty> volume_property = vtkSmartPointer<vtkVolumeProperty>::New();
	volume_property->SetColor(color_transfer_function);
	volume_property->SetScalarOpacity(opacity_transfer_function);
	volume_property->ShadeOn();
	volume_property->SetInterpolationTypeToLinear();
	
	vtkSmartPointer<vtkFixedPointVolumeRayCastMapper> volume_mapper = vtkSmartPointer<vtkFixedPointVolumeRayCastMapper>::New();
	
	#if VTK_MAJOR_VERSION <= 5
		volume_mapper->SetInput(vtk_image);
	#else
		volume_mapper->SetInputData(vtk_image);
	#endif
	volume_mapper->SetBlendModeToComposite();
	
	vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
	volume->SetMapper(volume_mapper); 
	volume->SetProperty(volume_property);
	volume->SetPosition(0, 0, 0);
	volume->SetPickable(0);
	volume->Update();


	// Preparing vtkPolyData to show the graph
	
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
	
	int point_counter = -1;
	for(int i = 0; i < this->allForestNodes.size(); i++){
		points->InsertNextPoint(this->allForestNodes[i].x, this->allForestNodes[i].y, this->allForestNodes[i].z);
		point_counter++;

		for(int j = 0; j < this->allForestNodes[i].branchIDs.size(); j++){
			points->InsertNextPoint(this->allForestNodes[this->allForestNodes[i].branchIDs[j]].x, 
				this->allForestNodes[this->allForestNodes[i].branchIDs[j]].y, this->allForestNodes[this->allForestNodes[i].branchIDs[j]].z);			
			point_counter++;
		}
		for(int j = 0; j < this->allForestNodes[i].branchIDs.size(); j++){
			vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
			line->GetPointIds()->SetId(0, point_counter - this->allForestNodes[i].branchIDs.size());
			line->GetPointIds()->SetId(1, point_counter - j);
			lines->InsertNextCell(line);
		}
	}
	
	unsigned char green[3] = {0, 255, 0};
	vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
	colors->SetNumberOfComponents(3);
	colors->InsertNextTupleValue(green);
	
	vtkSmartPointer<vtkPolyData> poly_data = vtkSmartPointer<vtkPolyData>::New();
	poly_data->SetPoints(points);
	poly_data->SetLines(lines);
	//poly_data->GetCellData()->SetScalars(colors);
	
	vtkSmartPointer<vtkPolyDataMapper> poly_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	#if VTK_MAJOR_VERSION <= 5
		poly_mapper->SetInput(poly_data);
	#else
		poly_mapper->SetInputData(poly_data);
	#endif

	vtkSmartPointer<vtkActor> poly_actor = vtkSmartPointer<vtkActor>::New();
	poly_actor->SetMapper(poly_mapper);
	poly_actor->GetProperty()->SetLineWidth(3);
	poly_actor->GetProperty()->SetColor(0, 255, 0);
	
	renderer->AddActor(poly_actor);

	//Add text
	/*for(int i = 0; i < this->allForestNodes.size(); i++){
		vtkSmartPointer<vtkTextActor> textActor = vtkSmartPointer<vtkTextActor>::New();
		vtkSmartPointer<vtkVectorText> text
		//textActor->SetPosition(
	}*/

	if(render_with_data)
		renderer->AddVolume(volume);

	renderer->ResetCamera();
	
	render_window_interactor->Initialize();
	render_window->Render();
	render_window_interactor->Start() ;

}
void ftkVesselTracer::ComputeMinimumSpanningTreeBoost(void){

	//vtkSmartPointer<vtkMutableUndirectedGraph> vtk_graph = vtkSmartPointer<vtkMutableUndirectedGraph>::New();
}

void ftkVesselTracer::ComputeMinimumSpanningForestWithLoopDetection(void){

	std::cout << "Creating minimum spanning forest.. " << this->edges.size() << std::endl;
	
	int from = 0, to = 0, label = -1, oldID = 0, newID = 0, loop_count = 0;
	for(int i = 0; i < this->edges.size(); i++){		
		from = this->edges[i].from;
		to = this->edges[i].to;
		
		//std::cout << i << std::endl;

		if(this->allForestNodes[from].ID  == -1 && this->allForestNodes[to].ID == -1){
			
			label++;
			
			this->forest.push_back(Tree(label, from, 2));

			this->allForestNodes[from].ID = label;
			this->allForestNodes[to].ID = label;

			this->allForestNodes[from].NBranches++;
			this->allForestNodes[from].branchIDs[this->allForestNodes[from].NBranches - 1] = to;

			this->allForestNodes[to].NBranches++;
			this->allForestNodes[to].branchIDs[this->allForestNodes[to].NBranches - 1] = from;
		}
		else if(this->allForestNodes[from].ID == -1 && this->allForestNodes[to].ID != -1){
			
			this->allForestNodes[from].ID = this->allForestNodes[to].ID;
			
			this->allForestNodes[from].NBranches++;
			this->allForestNodes[from].branchIDs[this->allForestNodes[from].NBranches - 1] = to;

			this->allForestNodes[to].NBranches++;
			this->allForestNodes[to].branchIDs[this->allForestNodes[to].NBranches - 1] = from;

			this->forest[this->allForestNodes[to].ID].NNodes++;
		}
		else if(this->allForestNodes[from].ID != -1 && this->allForestNodes[to].ID == -1){
			
			this->allForestNodes[to].ID = this->allForestNodes[from].ID;
			
			this->allForestNodes[from].NBranches++;
			this->allForestNodes[from].branchIDs[this->allForestNodes[from].NBranches - 1] = to;

			this->allForestNodes[to].NBranches++;
			this->allForestNodes[to].branchIDs[this->allForestNodes[to].NBranches - 1] = from;

			this->forest[this->allForestNodes[from].ID].NNodes++;
		}
		else{
			if(this->allForestNodes[to].ID < this->allForestNodes[from].ID){
				
				oldID = this->allForestNodes[from].ID;
				newID = this->allForestNodes[to].ID;
				
				this->RelabelForestNodes(oldID, newID);

				this->forest[newID].NNodes = this->forest[newID].NNodes + this->forest[oldID].NNodes;
				this->forest[oldID].NNodes = 0;

				this->allForestNodes[from].NBranches++;
				this->allForestNodes[from].branchIDs[this->allForestNodes[from].NBranches - 1] = to;

				this->allForestNodes[to].NBranches++;
				this->allForestNodes[to].branchIDs[this->allForestNodes[to].NBranches - 1] = from;
			}
			else if(this->allForestNodes[to].ID > this->allForestNodes[from].ID){

				oldID = this->allForestNodes[to].ID;
				newID = this->allForestNodes[from].ID;

				this->RelabelForestNodes(oldID, newID);

				this->forest[newID].NNodes = this->forest[newID].NNodes + this->forest[oldID].NNodes;
				this->forest[oldID].NNodes = 0;
				
				this->allForestNodes[from].NBranches++;
				this->allForestNodes[from].branchIDs[this->allForestNodes[from].NBranches - 1] = to;

				this->allForestNodes[to].NBranches++;
				this->allForestNodes[to].branchIDs[this->allForestNodes[to].NBranches - 1] = from;
			}
			else if(this->CheckNeighbors(from, to) == false){
				loop_count++;
				this->loops.push_back(AffinityEdge(from, to, 0.0));	
			}
		}
	}
	
	std::cout << "Forest created with " << this->edges.size() - loop_count << " edges. " << std::endl;

	

	//Removing redundant branches
	for(int i = 0; i < this->allForestNodes.size(); i++){
		for(int j = 0; j < this->allForestNodes[i].branchIDs.size(); j++){
			if(this->allForestNodes[i].branchIDs[j] == -1){
				this->allForestNodes[i].branchIDs.erase(this->allForestNodes[i].branchIDs.begin() + j, this->allForestNodes[i].branchIDs.end());
				break;
			}
		}
	}
	
	//this->PrintForest();
	this->VisualizeMinimumSpanningForest(true);

	/*int count = -1;
	for(int i = 0; i < label; i++){
		if(this->forest[i].NNodes > 0){
			count++;
			this->forest[count].NNodes = this->forest[i].NNodes;
			this->forest[count].start = this->forest[i].start;

			this->forest[count].ID = i;
		}
		else
			this->forest.erase(this->forest.begin() + i);
	}

	this->VisualizeMinimumSpanningForest();*/
}

void ftkVesselTracer::RelabelForestNodes(int oldID, int newID){
	
	std::vector<int> connected_nodes;
	this->GetTree(oldID, connected_nodes);

	if(connected_nodes.empty() == false){
		for(int i = 0; i < connected_nodes.size(); i++)
			this->allForestNodes[connected_nodes[i]].ID = newID;
	}
}

void ftkVesselTracer::GetTree(int ID2, std::vector<int>& connected_nodes){

	int root = this->forest[ID2].start, ID = this->forest[ID2].ID, currentNNodes = 0, totalNNodes = 0, childNodeID = 0;
	
	if(this->allForestNodes[root].ID != ID)
		return;
	
	connected_nodes.resize(this->allParams.graphAndMSTParams.maxTreeNodes + 100, 0);
	connected_nodes[currentNNodes] = root;

	while(currentNNodes <= totalNNodes){
		
		for(int i = 0; i < this->allForestNodes[connected_nodes[currentNNodes]].NBranches; i++){
			childNodeID = this->allForestNodes[connected_nodes[currentNNodes]].branchIDs[i];
			
			//if(this->allForestNodes[childNodeID].ID == ID && std::find(connected_nodes.begin(), connected_nodes.begin() + totalNNodes, childNodeID) == connected_nodes.end()){
			if(this->allForestNodes[childNodeID].ID == ID && ftkVesselTracer::IsInList(connected_nodes, totalNNodes, childNodeID) == false){
				totalNNodes++;
				connected_nodes[totalNNodes] = childNodeID;
			}
		}
		currentNNodes++;
	}

	connected_nodes.erase(connected_nodes.begin() + totalNNodes + 1, connected_nodes.end());

	if(this->forest[ID2].NNodes != connected_nodes.size()){
		std::cout << "Error in scanning tree ID = " << ID << " NNodes = " << this->forest[ID2].NNodes << " Connected nodes = ";
		for(int i = 0; i < connected_nodes.size(); i++)
			std::cout << " " << connected_nodes[i];
		std::cout << std::endl;
	}
}

bool ftkVesselTracer::CheckNeighbors(int ID1, int ID2){

	bool is_neighbor = false;
	
	for(int i = 0; i < this->allForestNodes[ID1].NBranches; i++){
		if(this->allForestNodes[ID1].branchIDs[i] == ID2)
			is_neighbor = true;
	}

	for(int i = 0; i < this->allForestNodes[ID2].NBranches; i++){
		if(this->allForestNodes[ID2].branchIDs[i] == ID1)
			is_neighbor = true;
	}
	return is_neighbor;
}

bool ftkVesselTracer::IsInList(std::vector<int> list, int last_item, int item_to_find){
	
	bool present = false;
	for(int i = 0; i <= last_item; i++){
		if(list[i] == item_to_find){
			present = true;
			break;
		}	
	}
	return present;
}

void ftkVesselTracer::PrintForest(void){
	
	std::vector<int> connected_nodes; 
	for(int i = 0; i < this->forest.size(); i++){
		std::cout << " Tree: " << i << " with " << this->forest[i].NNodes << " elements [";
		
		if(connected_nodes.empty() == false)
			connected_nodes.clear();
		
		this->GetTree(i, connected_nodes);

		if(connected_nodes.empty() == false){
			for(int j = 0; j < connected_nodes.size(); j++)
				std::cout << connected_nodes[j] << " ";
			std::cout << "] " << std::endl;
		}
		else
			std::cout << "NULL]" << std::endl;
	}
}

void ftkVesselTracer::InitNodeDetectionParamsDefault(void){
	
	this->allParams.nodeDetectionParams.initByDefaultValues();
}


SWCNodeVessel::SWCNodeVessel(){
	this->ID = -3;
	this->scale = 0.0;
	this->isLeaf = false;
	this->isBifurgation = false;
	this->isTrifurgation = false;
	this->isMultifurgation = false;
	this->isOrphan = false;
	this->isRoot = false;
}

void ftkVesselTracer::PrintToySWCFile(void){
	
	// Toy graph for testing
	std::vector<std::vector<int> > toy_list;
	std::vector<int> row0; row0.push_back(-2);
	std::vector<int> row1; row1.push_back(2); row1.push_back(3); row1.push_back(4); 
	std::vector<int> row2; row2.push_back(1);
	std::vector<int> row3; row3.push_back(1);
	std::vector<int> row4; row4.push_back(1); row4.push_back(6); row4.push_back(8); row4.push_back(5);
	std::vector<int> row5; row5.push_back(4); row5.push_back(9);
	std::vector<int> row6; row6.push_back(4); row6.push_back(7);
	std::vector<int> row7; row7.push_back(6); row7.push_back(8);
	std::vector<int> row8; row8.push_back(4); row8.push_back(7); row8.push_back(9);
	std::vector<int> row9; row9.push_back(8); row9.push_back(5); row9.push_back(10);
	std::vector<int> row10; row10.push_back(9);
	std::vector<int> row11; row11.push_back(12);
	std::vector<int> row12; row12.push_back(11); row12.push_back(13);
	std::vector<int> row13; row13.push_back(12); row13.push_back(14);
	std::vector<int> row14; row14.push_back(13); //row14.push_back(11);
	std::vector<int> row15; row15.push_back(16);
	std::vector<int> row16; row16.push_back(15);
	//std::vector<int> row19; row19.push_back(-2);

	toy_list.push_back(row0);
	toy_list.push_back(row1); toy_list.push_back(row2); toy_list.push_back(row3); toy_list.push_back(row4);
	toy_list.push_back(row5); toy_list.push_back(row6); toy_list.push_back(row7); toy_list.push_back(row8);
	toy_list.push_back(row9); toy_list.push_back(row10); toy_list.push_back(row11); toy_list.push_back(row12);
	toy_list.push_back(row13); toy_list.push_back(row14); toy_list.push_back(row15); toy_list.push_back(row16);
	//toy_list.push_back(row19); 

	// Toy vtkgraph 
	vtkSmartPointer<vtkMutableUndirectedGraph> toy_graph = vtkSmartPointer<vtkMutableUndirectedGraph>::New();
	vtkIdType v1 = toy_graph->AddVertex();
	vtkIdType v2 = toy_graph->AddVertex();
	vtkIdType v3 = toy_graph->AddVertex();
	vtkIdType v4 = toy_graph->AddVertex();
	vtkIdType v5 = toy_graph->AddVertex();
	vtkIdType v6 = toy_graph->AddVertex();
	vtkIdType v7 = toy_graph->AddVertex();
	vtkIdType v8 = toy_graph->AddVertex();
	vtkIdType v9 = toy_graph->AddVertex();
	vtkIdType v10 = toy_graph->AddVertex();
	vtkIdType v11 = toy_graph->AddVertex();
	vtkIdType v12 = toy_graph->AddVertex();
	vtkIdType v13 = toy_graph->AddVertex();
	vtkIdType v14 = toy_graph->AddVertex();
	vtkIdType v15 = toy_graph->AddVertex();
	vtkIdType v16 = toy_graph->AddVertex();
	vtkIdType v17 = toy_graph->AddVertex();


	toy_graph->AddEdge(v1, v2);
	toy_graph->AddEdge(v1, v3);
	toy_graph->AddEdge(v1, v4);
	toy_graph->AddEdge(v2, v1);
	toy_graph->AddEdge(v3, v1);
	toy_graph->AddEdge(v4, v1);
	toy_graph->AddEdge(v4, v6);
	toy_graph->AddEdge(v4, v8);
	toy_graph->AddEdge(v4, v5);
	toy_graph->AddEdge(v5, v4);
	toy_graph->AddEdge(v5, v9);
	toy_graph->AddEdge(v6, v4);
	toy_graph->AddEdge(v6, v7);
	toy_graph->AddEdge(v7, v6);
	toy_graph->AddEdge(v7, v8);
	toy_graph->AddEdge(v8, v4);
	toy_graph->AddEdge(v8, v7);
	toy_graph->AddEdge(v8, v9);
	toy_graph->AddEdge(v9, v8);
	toy_graph->AddEdge(v9, v5);
	toy_graph->AddEdge(v9, v10);
	toy_graph->AddEdge(v10, v9);
	toy_graph->AddEdge(v11, v12);
	toy_graph->AddEdge(v12, v11);
	toy_graph->AddEdge(v12, v13);
	toy_graph->AddEdge(v13, v12);
	toy_graph->AddEdge(v13, v14);
	toy_graph->AddEdge(v14, v13);
	toy_graph->AddEdge(v15, v16);
	toy_graph->AddEdge(v16, v15);
	
		
	vtkSmartPointer<vtkDirectedGraph> toy_graph_dir = vtkSmartPointer<vtkDirectedGraph>::New();
	bool isConverted = toy_graph->ToDirectedGraph(toy_graph_dir);

	std::cout << "Conversion valid: " << isConverted << std::endl;

	//toy_graph_dir->Print(std::cout);

	/*vtkIdType out1 = toy_graph->GetOutDegree(v7);
	vtkIdType out2 = toy_graph_dir->GetOutDegree(v7);
	vtkIdType in1 = toy_graph->GetInDegree(v7);
	vtkIdType in2 = toy_graph_dir->GetInDegree(v7);
	std::cout << "Before conversion out: " << out1 << std::endl;
	std::cout << "After conversion out: " << out2 << std::endl;
	
	std::cout << "Before conversion in: " << in1 << std::endl;
	std::cout << "After conversion in: " << in2 << std::endl;

	vtkSmartPointer<vtkEdgeListIterator> list_it_1 = vtkSmartPointer<vtkEdgeListIterator>::New();
	vtkSmartPointer<vtkEdgeListIterator> list_it_2 = vtkSmartPointer<vtkEdgeListIterator>::New();
	toy_graph->GetEdges(list_it_1);
	toy_graph_dir->GetEdges(list_it_2);
	std::cout << "Before: " << std::endl;
	while(list_it_1->HasNext()){
		vtkEdgeType edge = list_it_1->Next();
		std::cout << "Edge: " << edge.Id << " is from " << "Source: " << edge.Source << " to Target: " << edge.Target << std::endl;
	}
	std::cout << "After: " << std::endl;
	while(list_it_2->HasNext()){
		vtkEdgeType edge = list_it_2->Next();
		std::cout << "Edge: " << edge.Id << " is from " << "Source: " << edge.Source << " to Target: " << edge.Target << std::endl;
	}*/

	/*vtkSmartPointer<vtkOutEdgeIterator> out_it_1 = vtkSmartPointer<vtkOutEdgeIterator>::New();
	vtkSmartPointer<vtkOutEdgeIterator> out_it_2 = vtkSmartPointer<vtkOutEdgeIterator>::New();
	vtkSmartPointer<vtkInEdgeIterator> in_it_1 = vtkSmartPointer<vtkInEdgeIterator>::New();
	vtkSmartPointer<vtkInEdgeIterator> in_it_2 = vtkSmartPointer<vtkInEdgeIterator>::New();
	*/

	/*toy_graph->GetOutEdges(v4, out_it_1);
	toy_graph_dir->GetOutEdges(v4, out_it_2);
	
	std::cout << "Before: " << std::endl;
	 while(out_it_1->HasNext()){ 

	    vtkOutEdgeType edge = out_it_1->Next();
	    std::cout << "Edge id: " << edge.Id << " Target: " << edge.Target << std::endl;
     }
	 std::cout << "After: " << std::endl;
	 while(out_it_2->HasNext()){ 

	    vtkOutEdgeType edge = out_it_2->Next();
	    std::cout << "Edge id: " << edge.Id <<" Target: " << edge.Target << std::endl;
     }*/

	/*toy_graph_dir->GetOutEdges(v4, out_it_1);
	toy_graph_dir->GetOutEdges(v5, out_it_2);
	toy_graph_dir->GetInEdges(v2, in_it_1);
	
	std::cout << "Out1: " << std::endl;
	while(out_it_1->HasNext()){ 

	    vtkOutEdgeType edge = out_it_1->Next();
	    std::cout << "Edge id: " << edge.Id << " Target: " << edge.Target << std::endl;
    }
	std::cout << "Out2: " << std::endl;
	while(out_it_2->HasNext()){ 

	    vtkOutEdgeType edge = out_it_2->Next();
	    std::cout << "Edge id: " << edge.Id << " Target: " << edge.Target << std::endl;
     }

	std::cout << "In: " << std::endl;
	while(in_it_1->HasNext()){ 

	    vtkInEdgeType edge = in_it_1->Next();
	    std::cout << "Edge id: " << edge.Id <<" Target: " << edge.Source << std::endl;
    }*/
	
	
	//Find connected components of a graph - apply on undirected graph
	/*vtkSmartPointer<vtkBoostConnectedComponents> connected_components = vtkSmartPointer<vtkBoostConnectedComponents>::New();
	connected_components->SetInput(toy_graph);
	connected_components->Update();
	
	vtkSmartPointer<vtkGraph> connected_graph = connected_components->GetOutput();

	vtkSmartPointer<vtkIntArray> components = vtkIntArray::SafeDownCast(
    connected_graph->GetVertexData()->GetArray("component"));
 
	std::cout << "Components: " << std::endl;
	for(vtkIdType i = 0; i < components->GetNumberOfTuples(); i++){

		int val = components->GetValue(i);
		std::cout << val << std::endl;
    }*/
	
			
	// Finding the connected components - does not work when components are not labelled serially
	/*std::vector<bool> isRoot(toy_list.size(), false);
	std::vector<int> label(toy_list.size(), -3);
	int lbl = 0;
	for(int i = 1; i < toy_list.size(); i++){
	
		if(label[i] == -3){
			lbl++;
			isRoot[i] = true;
			label[i] = lbl;
			std::cout << "new label: " << lbl << std::endl;
		}

		std::cout << "i: " << i << std::endl;
		std::vector<int> a_row = toy_list[i];
		for(int j = 0; j < a_row.size(); j++){
		
			if(a_row[j] == -2)
				continue;

			if(label[a_row[j]] != -3)
				continue;

			label[a_row[j]] = lbl;
			
			std::cout << "j: " << a_row[j] << std::endl;
		}
	}*/
	
	
	//Print the labeled graph
	/*std::cout << "Labeled graph: " << std::endl;
	for(int i = 1; i < toy_list.size(); i++){		
		std::cout << i << " : " << label[i] << " -> ";
		for(int j = 0; j < toy_list[i].size(); j++){
			if(toy_list[i][j] < 0)
				continue;
			std::cout << toy_list[i][j] << " : " << label[toy_list[i][j]] << ", ";
		}
		std::cout << std::endl;
	}
	std::cout << "Roots: " << std::endl;
	for(int i = 1; i < toy_list.size(); i++){
		if(isRoot[i])
			std::cout << i << std::endl;
	}*/

	
	// Implement a queue based method for finding connected components of a graph
	
	std::vector<std::vector<int> > connected_components;
	std::queue<int> label_queue;
	std::vector<bool> isRoot(toy_list.size(), false);
	
	for(int i = 1; i < toy_list.size();){
		
		std::vector<int> a_component; 
		a_component.push_back(i);

		std::vector<int> connections = toy_list[i];
		for(int j = 0; j < connections.size(); j++){
			label_queue.push(connections[j]);
		}

		while(!label_queue.empty()){
			
			int cur_label = label_queue.front();
			label_queue.pop();

			if(std::find(a_component.begin(), a_component.end(), cur_label) != a_component.end())
				continue;
			
			a_component.push_back(cur_label);

			std::vector<int> neighbors = toy_list[cur_label];
			std::vector<int>::iterator itr;	
			for(int j = 0; j < neighbors.size(); j++){
				if(std::find(a_component.begin(), a_component.end(), neighbors[j]) != a_component.end())
					continue;

				label_queue.push(neighbors[j]);
			}
		}
		connected_components.push_back(a_component);
		isRoot[a_component.front()] = true;
		i = a_component.back();
		i++;
	}

	// Print the connected components
	std::cout << "Connected components.." << std::endl;
	for(int i = 0; i < connected_components.size(); i++){
		std::cout << std::endl;
		for(int j = 0; j < connected_components[i].size(); j++){

			std::cout << connected_components[i][j] << std::endl;
		}
	}
	


	// Constructing SWC nodes
	SWCNodeVessel node_dummy;
	SWCNodeVessel_vec.resize(toy_list.size(), node_dummy);
	//SWCNodeVessel_vec.push_back(node_dummy);
	SWCNodeVessel_vec[0] = node_dummy;

	for(int i = 1; i < toy_list.size(); i++){

		//if(i == 1){
		if(isRoot[i]){

			SWCNodeVessel node;
			node.ID = i;
			node.parents.push_back(-1);
			node.isRoot = true;
			//SWCNodeVessel_vec.push_back(node);
			SWCNodeVessel_vec[i] = node;
		}

		std::vector<int> a_row = toy_list[i];
		std::vector<int> isRelated(a_row.size(), false);
		for(int j = 0; j < a_row.size(); j++){

			if(!SWCNodeVessel_vec[i].children.empty()){
				for(int k = 0; k < SWCNodeVessel_vec[i].children.size(); k++){
					if(a_row[j] == SWCNodeVessel_vec[i].children[k]){
						
						//std::cout << "Related: " << i << " hasChild: " << a_row[j] << std::endl;

						isRelated[j] = true;
						break;
					}
				}
			}
			if(!SWCNodeVessel_vec[i].parents.empty()){
				for(int k = 0; k < SWCNodeVessel_vec[i].parents.size(); k++){
					if(a_row[j] == SWCNodeVessel_vec[i].parents[k]){

						//std::cout << "Related: " << i << " hasParent: " << a_row[j] << std::endl;

						isRelated[j] = true;
						break;
					}
				}
			}

			if(!isRelated[j]){

				//std::cout << i << ": " << a_row[j] << std::endl;

				if(a_row[j] < 0)
					continue;
				
				SWCNodeVessel node;
				if(SWCNodeVessel_vec[a_row[j]].ID > 0) // Check if node already exists in the list
					node = SWCNodeVessel_vec[a_row[j]];	
				
				node.ID = a_row[j];
				node.parents.push_back(i);
				//SWCNodeVessel_vec.push_back(node);
				SWCNodeVessel_vec[a_row[j]] = node;

				SWCNodeVessel_vec[i].children.push_back(a_row[j]);

				//std::cout << i << " hasChild: " << a_row[j] << std::endl;

			}
		}
		
		//for(int j = 0; j < SWCNodeVessel_vec[i].children.size(); j++)
		//	std::cout << i << " hasChild: " << SWCNodeVessel_vec[i].children[j] << std::endl;
	}
	
	std::cout << "SWC file: " << std::endl;
	for(int i = 1; i < SWCNodeVessel_vec.size(); i++){
		std::cout << "Node: " << std::endl;
		std::cout << "	ID: " << SWCNodeVessel_vec[i].ID << std::endl;
		std::cout << "	Parents: ";
		for(int j = 0; j < SWCNodeVessel_vec[i].parents.size(); j++)
			std::cout << SWCNodeVessel_vec[i].parents[j] << ", ";
		std::cout << std::endl;
		std::cout << "	Children: ";
		for(int j = 0; j < SWCNodeVessel_vec[i].children.size(); j++)
			std::cout << SWCNodeVessel_vec[i].children[j] << ", ";
		std::cout << std::endl;
	}
}

void ftkVesselTracer::PopulateSWCNodeContainerAndComputeNodeFeatures(void){

	if(this->allForestNodes.empty()){
		std::cout << "Forest nodes are empty! Returning. " << std::endl;
		return;
	}

	if(!this->SWCNodeVessel_vec.empty()){
		std::cout << "SWC container is emptied! " << std::endl;
		this->SWCNodeVessel_vec.clear();
	}
	
	// Print the forest to a file
	/*std::ofstream file_out;
	file_out.open("graph_file.txt", std::ios::out);
	if(file_out.good()){

		for(int i = 0; i < this->allForestNodes.size(); i++){
			file_out << i << " -> ";
			for(int j = 0; j < this->allForestNodes[i].branchIDs.size(); j++){
				file_out << this->allForestNodes[i].branchIDs[j] << ", ";
		}

		file_out << std::endl;
	}
	
	file_out.close();
	}
	else
		std::cout << "Could not write graph file! " << std::endl;
	*/
	
	//Labeling the forest
	//this->allForestNodes.insert(this->allForestNodes.begin(), Node());
	//this->allForestNodes[0].branchIDs.push_back(-2);


	// Node-based features - Branching
	for(int i = 0; i < this->allForestNodes.size(); i++){
		if(this->allForestNodes[i].branchIDs.size() == 3)
			this->allForestNodes[i].isBifurgation = true;
		if(this->allForestNodes[i].branchIDs.size() == 4)
			this->allForestNodes[i].isTrifurgation = true;
		if(this->allForestNodes[i].branchIDs.size() > 4)
			this->allForestNodes[i].isMultifurgation = true;
	}

	std::cout << "Finding connected components of the network... " << std::endl;

	std::vector<Node> dup_allForestNodes(this->allForestNodes);
	std::vector<std::vector<int> > connected_components;
	std::queue<int> label_queue;
	std::vector<bool> isRoot(this->allForestNodes.size(), false);
	std::vector<bool> isLabeled(this->allForestNodes.size(), false); //isLabeled[0] = true;
	bool allLabeled = false;
	int i = 0; //1;
	int component_count = 0;

	//for(int i = 1; i < dup_allForestNodes.size();){
	while(!allLabeled){

		std::vector<int> a_component; 
		a_component.push_back(i);
		isLabeled[i] = true;
		component_count++;
		this->allForestNodes[i].forestLabel = component_count;

		//std::cout << "Formed a new component.. " << component_count << std::endl;

		//if(this->allForestNodes[i].branchIDs.empty()){
			//std::cout << "Branch ids empty!! " << std::endl;
			//continue;
			//break;
		//}

		std::vector<int> connections = this->allForestNodes[i].branchIDs; //toy_list[i];
		//if(!connections.empty()){
			for(int j = 0; j < connections.size(); j++)
				label_queue.push(connections[j]);
		//}

		while(!label_queue.empty()){
			
			int cur_label = label_queue.front();
			label_queue.pop();

			if(std::find(a_component.begin(), a_component.end(), cur_label) != a_component.end())
				continue;
			
			a_component.push_back(cur_label);
			isLabeled[cur_label] = true;
			this->allForestNodes[cur_label].forestLabel = component_count;

			std::vector<int> neighbors = this->allForestNodes[cur_label].branchIDs; //toy_list[cur_label];
			std::vector<int>::iterator itr;	
			for(int j = 0; j < neighbors.size(); j++){
				if(std::find(a_component.begin(), a_component.end(), neighbors[j]) != a_component.end())
					continue;

				label_queue.push(neighbors[j]);
			}
		}
		connected_components.push_back(a_component);
		isRoot[a_component.front()] = true;
		this->allForestNodes[a_component.front()].isRoot = true;

		//i = a_component.back();
		//i++;
		
		//if(!a_component.empty()){
			//std::cout << "Component: " << std::endl;
			//for(int j = 0; j < a_component.size(); j++)
				//std::cout << a_component[j] << std::endl;
		//}
		//allLabeled = true;
		
		int i_past = i;
		bool unlabeled_found = false;
		for(int j = 0; j < this->allForestNodes.size(); j++){

			if(j == this->allForestNodes.size()-1){
				allLabeled = true;
				break;
			}

			if(isLabeled[j])
				continue;
			else{
				i = j;
				unlabeled_found = true;
				//break;
			}
			if(unlabeled_found){
				//std::cout << "Found unlabeled element: " << j << std::endl;
				break;
			}
			else{
				//std::cout << "No unlabeled element found!! " << std::endl; 
				allLabeled = true;
			}

			//if(i == i_past)
			//if(j == this->allForestNodes.size())
			//	allLabeled = true;
		}

		//std::cout << " Total unlabeled status: " << allLabeled << std::endl;
	}

	//Print the labeled graph
	/*std::cout << "Labeled graph: " << std::endl;
	for(int i = 1; i < this->allForestNodes.size(); i++){		
		std::cout << i << " : " << this->allForestNodes[i].forestLabel << " -> ";
		for(int j = 0; j < this->allForestNodes[i].branchIDs.size(); j++){
			if(this->allForestNodes[i].branchIDs[j] < 0)
				continue;
			std::cout << this->allForestNodes[i].branchIDs[j] << " : " << this->allForestNodes[this->allForestNodes[i].branchIDs[j]].forestLabel << ", ";
		}
		std::cout << std::endl;
	}
	std::cout << "Roots: " << std::endl;
	for(int i = 1; i < this->allForestNodes.size(); i++){
		if(this->allForestNodes[i].isRoot)
			std::cout << i << std::endl;
	}*/
	
	std::cout << "Converting the network to a directed graph... " << std::endl;

	// Construct SWC nodes
	SWCNodeVessel node_dummy;
	std::vector<std::vector<SWCNodeVessel> > SWCNodeVec_byComponent;
	
	for(int i = 0; i < connected_components.size(); i++){
		
		std::vector<SWCNodeVessel> node_vec_dummy, node_vec_component; 
		node_vec_dummy.resize(this->allForestNodes.size(), node_dummy);
		std::vector<int> curr_component = connected_components[i];
		std::vector<int> curr_component_order;

		for(int j = 0; j < curr_component.size(); j++){
			
			int curr_ID = curr_component[j];
			if(this->allForestNodes[curr_ID].isRoot){

				SWCNodeVessel node;
				node.ID = curr_ID;
				node.parents.push_back(-1);
				node.isRoot = true;

				itk::Index<3> idx; idx[0] = this->allForestNodes[curr_ID].x; 
				idx[1] = this->allForestNodes[curr_ID].y; idx[2] = this->allForestNodes[curr_ID].z; 

				node.position = idx;
				node.scale = this->allForestNodes[curr_ID].scale;

				node_vec_dummy[curr_ID] = node;
				curr_component_order.push_back(curr_ID);
			}
			
			std::vector<int> curr_connections = this->allForestNodes[curr_ID].branchIDs;
			std::vector<bool> isRelated(curr_connections.size(), false);

			for(int k = 0; k < curr_connections.size(); k++){
				
				for(int m = 0; m < node_vec_dummy[curr_ID].children.size(); m++){
					if(curr_connections[k] == node_vec_dummy[curr_ID].children[m]){
						isRelated[k] = true;
						break;
					}
				}
				for(int m = 0; m < node_vec_dummy[curr_ID].parents.size(); m++){
					if(curr_connections[k] == node_vec_dummy[curr_ID].parents[m]){
						isRelated[k] = true;
						break;
					}
				}

				if(!isRelated[k]){
					
					SWCNodeVessel node;
					if(node_vec_dummy[curr_connections[k]].ID >= 0)
						node = node_vec_dummy[curr_connections[k]];

					node.ID = curr_connections[k];
					node.parents.push_back(curr_ID);

					itk::Index<3> idx; idx[0] = this->allForestNodes[curr_connections[k]].x; 
					idx[1] = this->allForestNodes[curr_connections[k]].y; idx[2] = this->allForestNodes[curr_connections[k]].z; 

					node.position = idx;
					node.scale = this->allForestNodes[curr_connections[k]].scale;

					node_vec_dummy[curr_connections[k]] = node;
					curr_component_order.push_back(curr_connections[k]);

					node_vec_dummy[curr_ID].children.push_back(curr_connections[k]);
				}
			}
		}

		for(int j = 0; j < curr_component_order.size(); j++){
			this->SWCNodeVessel_vec.push_back(node_vec_dummy[curr_component_order[j]]);
			node_vec_component.push_back(node_vec_dummy[curr_component_order[j]]);
		}
		//SWCNodeVec_byComponent.push_back(node_vec_component);

		// Node-based features - Identify leaf points and orphans
		for(int j = 0; j < curr_component.size(); j++){
			if(node_vec_component.size() > 1){	

				bool is_leaf = true;
				for(int k = 0; k < node_vec_component.size(); k++){
					for(int m = 0; m < node_vec_component[k].parents.size(); m++){
						if(curr_component[j] == node_vec_component[k].parents[m]){
							is_leaf = false;
							break;
						}
					}
					if(!is_leaf)
						break;
				}
				if(is_leaf)
					this->allForestNodes[curr_component[j]].isLeaf = true;
			}
			if(node_vec_component.size() == 1)
				this->allForestNodes[curr_component[j]].isOrphan = true;
		}

		for(int j = 0; j < node_vec_component.size(); j++){
			this->allForestNodes[node_vec_component[j].ID].children = node_vec_component[j].children;
			this->allForestNodes[node_vec_component[j].ID].parents = node_vec_component[j].parents;
		}

		node_vec_dummy.clear();
		curr_component.clear();
		curr_component_order.clear();
		node_vec_component.clear();
	}


	// Massage the SWC file
	for(int i = 0; i < this->SWCNodeVessel_vec.size(); i++){
		
		this->SWCNodeVessel_vec[i].ID++;
		
		for(int j = 0; j < this->SWCNodeVessel_vec[i].children.size(); j++)
			this->SWCNodeVessel_vec[i].children[j]++;
		for(int j = 0; j < this->SWCNodeVessel_vec[i].parents.size(); j++){
			if(this->SWCNodeVessel_vec[i].parents[j] != -1)
				this->SWCNodeVessel_vec[i].parents[j]++;
		}
	}
	
	// Printing the SWC file to cout
	/*std::cout << "SWC file: " << std::endl;
	for(int i = 0; i < SWCNodeVessel_vec.size(); i++){
		std::cout << "Node: " << std::endl;
		std::cout << "	ID: " << SWCNodeVessel_vec[i].ID << std::endl;
		std::cout << "	Parents: ";
		for(int j = 0; j < SWCNodeVessel_vec[i].parents.size(); j++)
			std::cout << SWCNodeVessel_vec[i].parents[j] << ", ";
		std::cout << std::endl;
		std::cout << "	Children: ";
		for(int j = 0; j < SWCNodeVessel_vec[i].children.size(); j++)
			std::cout << SWCNodeVessel_vec[i].children[j] << ", ";
		std::cout << std::endl;
	}*/

	std::cout << "Done with computing the SWC file structure. " << std::endl;
}

void ftkVesselTracer::WriteSWCFileVessel(void){

	if(this->SWCNodeVessel_vec.empty()){
		std::cout << "SWCNode container is empty. Returning." << std::endl;
		return;
	}

	std::string swc_file_name = this->data_folder_path;
	swc_file_name.append("_vesselTraces.swc");

	//Write SWC file
	std::ofstream file_out;
	file_out.open(swc_file_name.c_str(), std::ios::out);
	if(file_out.good()){

		for(int i = 0; i < this->SWCNodeVessel_vec.size(); i++){
			for(int j = 0; j < this->SWCNodeVessel_vec[i].parents.size(); j++){
				
				file_out << this->SWCNodeVessel_vec[i].ID << " " << "7" << " " << this->SWCNodeVessel_vec[i].position[0] << " ";
				file_out << this->SWCNodeVessel_vec[i].position[1] << " " << this->SWCNodeVessel_vec[i].position[2] << " ";
				file_out << this->SWCNodeVessel_vec[i].scale << " " << this->SWCNodeVessel_vec[i].parents[j] << std::endl;
			}
		}

		file_out.close();

		std::cout << "Done with writing the SWC file: " << swc_file_name << std::endl;
	}
	else
		std::cout << "Could not write SWC file! " << std::endl;
}


void ftkVesselTracer::ComputeVesselnessImage(VesselnessMeasures& obj_measures, std::string& write_file_path, ImageType3D::Pointer& data_ptr){

	//float sigma_min = 2.0f; //0.5f;
	//float sigma_max = 10.0f; //4.0f;
	//int sigma_steps = 5;

	//float alpha = 0.5, beta = 0.5, gamma = 0.25; //5.0;

	//int obj_dim = objectness_type; //1; //0: Blobness, 1: Vesselness, 2: Plateness

	std::cout << "Computing vesselness..."  << std::endl;

	StatisticsFilterType::Pointer stats_filter = StatisticsFilterType::New();
	stats_filter->SetInput(data_ptr);
	stats_filter->Update();
	double img_max_val = stats_filter->GetMaximum();

	obj_measures.alpha = obj_measures.alpha * img_max_val;
	obj_measures.beta = obj_measures.beta * img_max_val;
	obj_measures.gamma = obj_measures.gamma * img_max_val;


	MultiScaleHessianFilterType::Pointer multi_scale_Hessian = MultiScaleHessianFilterType::New();
	multi_scale_Hessian->SetInput(data_ptr);
	multi_scale_Hessian->SetSigmaMin(obj_measures.sigma_min);
	multi_scale_Hessian->SetSigmaMax(obj_measures.sigma_max);
	multi_scale_Hessian->SetNumberOfSigmaSteps(obj_measures.sigma_intervals);

	//ObjectnessFilterType::Pointer objectness_filter = ObjectnessFilterType::New();
	ObjectnessFilterType::Pointer objectness_filter = multi_scale_Hessian->GetHessianToMeasureFilter();
	
	objectness_filter->SetScaleObjectnessMeasure(false);
	objectness_filter->SetBrightObject(true);
	objectness_filter->SetAlpha(obj_measures.alpha);
	objectness_filter->SetBeta(obj_measures.beta);
	objectness_filter->SetGamma(obj_measures.gamma);
	objectness_filter->SetObjectDimension(obj_measures.vesselness_type);
	
	//std::cout << obj_measures.alpha << std::endl << obj_measures.beta << std::endl << obj_measures.gamma << std::endl;

	multi_scale_Hessian->Update();

	Common::NormalizeData(multi_scale_Hessian->GetOutput(), this->VesselnessImage);
	
	//this->VesselnessImage = normalization_filter->GetOutput();

	std::cout << "Writing vesselness image.." << std::endl;

	std::string output_file;
	output_file = write_file_path + "_vesselness.mhd";
	Common::WriteImage3D(output_file, this->VesselnessImage);		
}

void ftkVesselTracer::WriteSkeletonImage(void){

	if(this->allForestNodes.empty()){
		std::cout << "Forest nodes container is empty. Returning." << std::endl;
		return;
	}
	
	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->skeletonImage = RenderImageType3D::New();
	this->skeletonImage->SetRegions(id_reg);
	this->skeletonImage->Allocate();
	this->skeletonImage->SetSpacing(this->inputData->GetSpacing());

	this->skeletonImage->FillBuffer(0);

	for(int i = 0; i < this->allForestNodes.size(); i++){

		itk::Index<3> pixel0;
		pixel0[0] = this->allForestNodes[i].x;
		pixel0[1] = this->allForestNodes[i].y;
		pixel0[2] = this->allForestNodes[i].z;

		if(!this->allForestNodes[i].children.empty()){

			for(int j = 0; j < this->allForestNodes[i].children.size(); j++){

				//std::cout << this->SWCNodeVessel_vec[i].children[j] << std::endl;
				
				LineType3D line;
				itk::Index<3> pixel1;
				pixel1[0] = this->allForestNodes[this->allForestNodes[i].children[j]].x;
				pixel1[1] = this->allForestNodes[this->allForestNodes[i].children[j]].y;
				pixel1[2] = this->allForestNodes[this->allForestNodes[i].children[j]].z;

				double euclid_distance = std::sqrt(std::pow((double)(pixel0[0] - pixel1[0]), 2) + std::pow((double)(pixel0[1] - pixel1[1]), 2) + std::pow((double)(pixel0[2] - pixel1[2]), 2));  

				if(euclid_distance < 1.0){
					//std::cout << "Distance between two nodes was less than 1 !! " << std::endl;
					//std::cout << "Parent: " << i << " at: " << pixel0 << " Child: " << this->allForestNodes[i].children[j] << " at: " << pixel1 << std::endl;
					this->skeletonImage->SetPixel(pixel0, 255);
					continue;
				}
				
				//std::cout << distance << std::endl;

				std::vector<itk::Index<3> > line_pixels = line.BuildLine(pixel0, pixel1);

				for(int k = 0; k < line_pixels.size(); k++){
				//	std::cout << line_pixels[k] << std::endl;
					this->skeletonImage->SetPixel(line_pixels[k], 255);
				}
				//std::cout << std::endl;
				//line_pixels.clear();

				//LineIteratorType3D line_it(this->skeletonImage, pixel0, pixel1);
				//line_it.GoToBegin();
				//while(!line_it.IsAtEnd())
				//	this->skeletonImage->SetPixel(line_it.GetIndex(), 255);
			}
		}
	}
	
	std::string skeleton_file_name = this->data_folder_path;
	skeleton_file_name.append("_SkeletonImage.tif");
	//std::cout << skeleton_file_name << std::endl;

	ImageWriter::Pointer skeleton_writer = ImageWriter::New();	
	skeleton_writer->SetFileName(skeleton_file_name);	
	skeleton_writer->SetInput(this->skeletonImage);
	skeleton_writer->Update();

	std::cout << "Done with writing the skeleton image: " << skeleton_file_name << std::endl;
}

void ftkVesselTracer::WriteSegmentationMask(void){

	if(this->allForestNodes.empty()){
		std::cout << "Forest nodes container is empty. Returning." << std::endl;
		return;
	}
	
	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->segmentationMaskImage = RenderImageType3D::New();
	this->segmentationMaskImage->SetRegions(id_reg);
	this->segmentationMaskImage->Allocate();
	this->segmentationMaskImage->SetSpacing(this->inputData->GetSpacing());

	this->segmentationMaskImage->FillBuffer(0);

	for(int i = 0; i < this->allForestNodes.size(); i++){

		double rad = this->allForestNodes[i].scale;
		itk::Index<3> idx;
		idx[0] = this->allForestNodes[i].x; idx[1] = this->allForestNodes[i].y; idx[2] = this->allForestNodes[i].z;

		RenderImageType3D::IndexType starting_index, end_index;
		RenderImageType3D::SizeType sub_volume_size;
		RenderImageType3D::RegionType sub_volume_region;
		RenderImageType3D::Pointer sub_volume;

		starting_index[0] = idx[0] - rad; starting_index[1] = idx[1] - rad; starting_index[2] = idx[2] - rad;
		end_index[0] = idx[0] + rad; end_index[1] = idx[1] + rad; end_index[2] = idx[2] + rad;
	
		if(starting_index[0] < 0)
			starting_index[0] = 0;
		if(starting_index[1] < 0)
			starting_index[1] = 0;
		if(starting_index[2] < 0)
			starting_index[2] = 0;
		if(end_index[0] > id_sz[0])
			end_index[0] = id_sz[0];
		if(end_index[1] > id_sz[1])
			end_index[1] = id_sz[1];
		if(end_index[2] > id_sz[2])
			end_index[2] = id_sz[2];

				
		for(int k = starting_index[0]; k < end_index[0]; k++){
			for(int l = starting_index[1]; l < end_index[1]; l++){
				for(int m = starting_index[2]; m < end_index[2]; m++){

					itk::Index<3> idx1;
					idx1[0] = k; idx1[1] = l; idx1[2] = m;

					double euclid_distance = std::sqrt(std::pow((double)(idx[0] - idx1[0]), 2) + std::pow((double)(idx[1] - idx1[1]), 2) + std::pow((double)(idx[2] - idx1[2]), 2));  
		
					if(euclid_distance <= rad)
						this->segmentationMaskImage->SetPixel(idx1, 255);
				}
			}
		}
	}

	std::string mask_file_name = this->data_folder_path;
	mask_file_name.append("_SegmentationMaskImage.tif");
	//std::cout << mask_file_name << std::endl;

	ImageWriter::Pointer mask_writer = ImageWriter::New();	
	mask_writer->SetFileName(mask_file_name);	
	mask_writer->SetInput(this->segmentationMaskImage);
	mask_writer->Update();

	std::cout << "Done with writing the segmentation mask image: " << mask_file_name << std::endl;
}

void ftkVesselTracer::WriteNodePropertiesFile(void){

	if(this->allForestNodes.empty()){
		std::cout << "Node container is empty. Returning. " << std::endl;
		return;
	}
	
	std::string prop_file_name = this->data_folder_path;
	prop_file_name.append("_nodeFeatures.txt");

	std::ofstream file_stream;
	file_stream.open(prop_file_name.c_str(), std::ios::out);

	if(file_stream.is_open() == true){

		file_stream << "ID" << '\t' << "x" << '\t' << "y" << '\t' << "z" << '\t' << "scale" << '\t' << "likelihood" << '\t' << "forest_label" << '\t';
		file_stream << "is_leaf" << '\t' << "is_root" << '\t' << "is_bifurgation" << '\t' << "is_trifurgation" << '\t' << "trace_quality" << '\t' << "ODF_modes" << '\t';
		file_stream << "ODF_mode_0_x" << '\t' << "ODF_mode_0_y" << '\t' << "ODF_mode_0_z" << '\t' << "ODF_mode_1_x" << '\t' << "ODF_mode_1_y" << '\t' << "ODF_mode_1_z" << '\t'; 
		file_stream << "ODF_mode_2_x" << '\t' << "ODF_mode_2_y" << '\t' << "ODF_mode_2_z" << '\t';
		file_stream << std::endl;

		//std::cout << this->allForestNodes.size() << std::endl;

		for(int i = 0; i < this->allForestNodes.size(); i++){
						
			file_stream << i << '\t' << this->allForestNodes[i].x << '\t' << this->allForestNodes[i].y << '\t' << this->allForestNodes[i].z << '\t';
			file_stream << this->allForestNodes[i].scale << '\t' << this->allForestNodes[i].likelihood << '\t' << this->allForestNodes[i].forestLabel << '\t';
			file_stream << this->allForestNodes[i].isLeaf << '\t' << this->allForestNodes[i].isRoot << '\t' << this->allForestNodes[i].isBifurgation << '\t';
			file_stream << this->allForestNodes[i].isTrifurgation << '\t' << this->allForestNodes[i].traceQuality << '\t';
			
			file_stream << this->allForestNodes[i].dirX.size() << '\t' << this->allForestNodes[i].dirX[0] << '\t' << this->allForestNodes[i].dirY[0] << '\t' << this->allForestNodes[i].dirZ[0] << '\t';

			if(this->allForestNodes[i].dirX.size() == 2){
				file_stream << this->allForestNodes[i].dirX[1] << '\t' << this->allForestNodes[i].dirY[1] << '\t' << this->allForestNodes[i].dirZ[1] << '\t';
				file_stream << 0.0 << '\t' << 0.0 << '\t' << 0.0 << '\t';
			}
			else if(this->allForestNodes[i].dirX.size() == 3){
				file_stream << this->allForestNodes[i].dirX[1] << '\t' << this->allForestNodes[i].dirY[1] << '\t' << this->allForestNodes[i].dirZ[1] << '\t';
				file_stream << this->allForestNodes[i].dirX[2] << '\t' << this->allForestNodes[i].dirY[2] << '\t' << this->allForestNodes[i].dirZ[2] << '\t';
			}
			else{
				file_stream << 0.0 << '\t' << 0.0 << '\t' << 0.0 << '\t';
				file_stream << 0.0 << '\t' << 0.0 << '\t' << 0.0 << '\t';
			}

			file_stream << std::endl;

			//for(int j = 0; j < this->allForestNodes[i].dirX.size(); j++)
			//	nodes_file_stream << this->allForestNodes[i].dirX[j] << '\t';
		}
		file_stream.close();

		std::cout << "Done with writing node properties file: " << prop_file_name << std::endl;
	}
	else
		std::cout << " Unable to open file for writing nodes: " << prop_file_name << std::endl;
}

void ftkVesselTracer::SmartRetrace(void){

	this->ComputeRetracingStartPoints();
}

void ftkVesselTracer::ComputeRetracingStartPoints(void){

	if(this->allForestNodes.empty()){
		std::cout << "Forest nodes container is emtpy. Returning. " << std::endl;
		return;
	}


	// Compute leaf and root points
	for(int i = 0; i < this->allForestNodes.size(); i++){
		
		if(this->allForestNodes[i].isLeaf || this->allForestNodes[i].isRoot || this->allForestNodes[i].isOrphan)
			this->retracingStartPoints.push_back(this->allForestNodes[i]);
	}

	std::cout << "Done with computing starting points for retracing: " << this->retracingStartPoints.size();

	// Compute possible correction lines
	int retrace_nhood = 100;
	for(int i = 0; i < this->retracingStartPoints.size(); i++){
		
		
	}

	// Write the retracing points image
	RenderImageType3D::RegionType id_reg;
	RenderImageType3D::IndexType id_st;
	RenderImageType3D::SizeType id_sz = this->inputData->GetBufferedRegion().GetSize();

	id_st[0] = 0;
	id_st[1] = 0;
	id_st[2] = 0;
	id_reg.SetSize(id_sz);
	id_reg.SetIndex(id_st);
	
	this->retracingStartPointsImage = RenderImageType3D::New();
	this->retracingStartPointsImage->SetRegions(id_reg);
	this->retracingStartPointsImage->Allocate();
	this->retracingStartPointsImage->SetSpacing(this->inputData->GetSpacing());
	this->retracingStartPointsImage->FillBuffer(0);

	itk::Index<3> idx;
	for(int i = 0; i < this->retracingStartPoints.size(); i++){
		
		idx[0] = this->retracingStartPoints[i].x;
		idx[1] = this->retracingStartPoints[i].y;
		idx[2] = this->retracingStartPoints[i].z;

		this->retracingStartPointsImage->SetPixel(idx, 255);
	}
	
	std::string retracing_start_nodes_file_name = this->data_folder_path;
	retracing_start_nodes_file_name.append("_RetracingStartPoints.tif");

	ImageWriter::Pointer retracing_start_nodes_writer = ImageWriter::New();	
	retracing_start_nodes_writer->SetFileName(retracing_start_nodes_file_name);	
	retracing_start_nodes_writer->SetInput(this->retracingStartPointsImage);
	retracing_start_nodes_writer->Update();
	
}

IntrinsicFeatureVector_VT::IntrinsicFeatureVector_VT(){
}

AssociativeFeatureVector_VT::AssociativeFeatureVector_VT(){

	this->minRootDist = 100000;
	this->maxRootDist = 100000;
	this->meanRootDist = 100000;
	this->varRootDist = 100000;
	this->nRoots = 0;

	this->distToVessel = 0.0;
	this->meanVesselDist = 10000;
	this->varVesselDist = 10000;
	this->alignmentWithVessel = -1;
	this->nVesselPoints = 0;
	this->overlapWithVessel = 0.0;
}

NucleiObject_VT::NucleiObject_VT(){

	IntrinsicFeatureVector_VT();
	AssociativeFeatureVector_VT();
}

VesselBasedNucleiFeatures::VesselBasedNucleiFeatures(){
}

VesselBasedNucleiFeatures::~VesselBasedNucleiFeatures(){
}

VesselBasedNucleiFeatures::VesselBasedNucleiFeatures(const std::string nuc_table_path, const std::string skeleton_img_path, const std::string node_prop_path, const std::string nuc_label_path, const std::string vessel_mask_path){

	this->data_folder_path = node_prop_path;
	this->data_folder_path.erase(this->data_folder_path.length()-4, this->data_folder_path.length());
	
	this->ReadNucleiFeatureTable(nuc_table_path);
	this->ReadSkeletonImage(skeleton_img_path);
	this->ReadNodePropertiesFile(node_prop_path);
	this->ReadNucleiLabelImage(nuc_label_path);
	this->ReadSegmentationMask(vessel_mask_path); 
	
	this->SetInsideRegionToGlobal();
	this->ComputeSkeletonDistanceMap();
	this->ComputeNucleiDistanceMap();

	this->ComputeVesselFeaturesForNuclei();
	this->WriteNucleiFeatureTable();
}

void VesselBasedNucleiFeatures::ReadNucleiFeatureTable(const std::string file_path){
	
	std::ifstream nucleiPoints;
	nucleiPoints.open(file_path.c_str(), std::ios::in); 
	std::cout << "Reading nuclei features file. " << std::endl;
	
	std::vector<std::string> str_vec;
	std::string line, str1;
	if(nucleiPoints.is_open()){
		
		unsigned short line_number = 0;
		NucleiObject_VT nuclei_object;

		while(nucleiPoints.good()){

			line_number++;
			//std::cout << line_number << std::endl;

			if(!std::getline(nucleiPoints, line))
				break;

			////Ignore the first line since it is all text
			if(line_number == 1)
				continue;
			
			std::istringstream str_stream(line); 
			while(str_stream.good()){
				
				if(!getline(str_stream, str1, '\t'))
					break;
				
				//// Crazy "nan" is replaced by -1
				if(strcmp(str1.c_str(), "nan") == 0)
					str1 = "-1";

				str_vec.push_back(str1);
			}

			nuclei_object.intrinsicFeatures.ID = atof(str_vec[0].c_str());

			itk::Index<3> idx;
			idx[0] = atof(str_vec[1].c_str());
			idx[1] = atof(str_vec[2].c_str());
			idx[2] = atof(str_vec[3].c_str());
			nuclei_object.intrinsicFeatures.centroid = idx;

			nuclei_object.intrinsicFeatures.volume = atof(str_vec[4].c_str());
			nuclei_object.intrinsicFeatures.integratedIntensity = atof(str_vec[5].c_str());
			nuclei_object.intrinsicFeatures.meanIntensity = atof(str_vec[6].c_str());
			nuclei_object.intrinsicFeatures.varianceIntensity = atof(str_vec[7].c_str());

			nuclei_object.intrinsicFeatures.eccentricity = atof(str_vec[8].c_str());
			nuclei_object.intrinsicFeatures.elongation = atof(str_vec[9].c_str());
			//nuclei_object.intrinsicFeatures.boundingBoxVolume = atof(str_vec[9].c_str());
			nuclei_object.intrinsicFeatures.boundingBoxVolume = nuclei_object.intrinsicFeatures.volume;

			nuclei_object.intrinsicFeatures.meanSurfaceGradient = atof(str_vec[10].c_str());
			nuclei_object.intrinsicFeatures.radiusVariation = atof(str_vec[11].c_str());
			nuclei_object.intrinsicFeatures.shapeMeasure = atof(str_vec[12].c_str());
			
			nuclei_object.intrinsicFeatures.energy = atof(str_vec[13].c_str());
			nuclei_object.intrinsicFeatures.entropy = atof(str_vec[14].c_str());
			nuclei_object.intrinsicFeatures.inverseDiffMoment = atof(str_vec[15].c_str());
			nuclei_object.intrinsicFeatures.inertia = atof(str_vec[16].c_str());
			nuclei_object.intrinsicFeatures.clusterShade = atof(str_vec[17].c_str());
			nuclei_object.intrinsicFeatures.clusterProminence = atof(str_vec[18].c_str());

			nuclei_object.associativeFeatures.astro_total = atof(str_vec[19].c_str());
			nuclei_object.associativeFeatures.astro_avg = atof(str_vec[20].c_str());
			nuclei_object.associativeFeatures.astro_surr = atof(str_vec[21].c_str());
			nuclei_object.associativeFeatures.micro_total = atof(str_vec[22].c_str());
			nuclei_object.associativeFeatures.micro_avg = atof(str_vec[23].c_str());
			nuclei_object.associativeFeatures.micro_surr = atof(str_vec[24].c_str());
			nuclei_object.associativeFeatures.neuro_total = atof(str_vec[25].c_str());
			nuclei_object.associativeFeatures.neuro_avg = atof(str_vec[26].c_str());
			nuclei_object.associativeFeatures.neuro_surr = atof(str_vec[27].c_str());
			nuclei_object.associativeFeatures.vessel_total = atof(str_vec[28].c_str());
			nuclei_object.associativeFeatures.vessel_avg = atof(str_vec[29].c_str());
			nuclei_object.associativeFeatures.vessel_surr = atof(str_vec[30].c_str());
			
			nuclei_object.associativeFeatures.minRootDist = atof(str_vec[31].c_str());
			nuclei_object.associativeFeatures.maxRootDist = atof(str_vec[32].c_str());
			nuclei_object.associativeFeatures.meanRootDist = atof(str_vec[33].c_str());
			nuclei_object.associativeFeatures.varRootDist = atof(str_vec[34].c_str());
			nuclei_object.associativeFeatures.nRoots = atof(str_vec[35].c_str());

			this->nucleiObjects.push_back(nuclei_object);

			str_vec.clear();
		}
		nucleiPoints.close();
	}
	else{
		std::cout << " Could not open nuclei features file. Exiting now. " << std::endl;
		return;
	}

	if(this->nucleiObjects.empty()){
		std::cout << " Empty nuclei features file. Returning. " << std::endl;
		return;
	}
	std::cout << "Nuclei features file read: " << this->nucleiObjects.size() << std::endl;
}

void VesselBasedNucleiFeatures::ReadSkeletonImage(const std::string file_path){

	Common::ReadImage3DUChar(file_path, this->skeletonImage);
} 

void VesselBasedNucleiFeatures::ReadNodePropertiesFile(const std::string file_path){

	std::ifstream nodePoints;
	nodePoints.open(file_path.c_str(), std::ios::in); 
	std::cout << "Reading vessel node features file. " << std::endl;
	
	std::vector<std::string> str_vec;
	std::string line, str1;
	if(nodePoints.is_open()){
		
		unsigned short line_number = 0;
		Node node_object;

		while(nodePoints.good()){

			line_number++;
			//std::cout << line_number << std::endl;

			if(!std::getline(nodePoints, line))
				break;

			//Ignore the first line since it is all text
			if(line_number == 1)
				continue;
			
			std::istringstream str_stream(line); 
			while(str_stream.good()){
				
				if(!getline(str_stream, str1, '\t'))
					break;
				
				// Crazy "nan" is replaced by -1
				//if(strcmp(str1.c_str(), "nan") == 0)
				//	str1 = "-1";

				str_vec.push_back(str1);
			}

			node_object.ID = atof(str_vec[0].c_str());

			node_object.x = atof(str_vec[1].c_str());
			node_object.y = atof(str_vec[2].c_str());
			node_object.z = atof(str_vec[3].c_str());
			
			node_object.scale = atof(str_vec[4].c_str());
			node_object.likelihood = atof(str_vec[5].c_str());

			node_object.forestLabel = atof(str_vec[6].c_str());
			node_object.isLeaf = atof(str_vec[7].c_str());
			node_object.isRoot = atof(str_vec[8].c_str());
			node_object.isBifurgation = atof(str_vec[9].c_str());
			node_object.isTrifurgation = atof(str_vec[10].c_str());

			node_object.traceQuality = atof(str_vec[11].c_str());
			node_object.ODF_modes = atof(str_vec[12].c_str());

			node_object.dirX.push_back(atof(str_vec[13].c_str()));
			node_object.dirY.push_back(atof(str_vec[14].c_str()));
			node_object.dirZ.push_back(atof(str_vec[15].c_str()));

			if(node_object.ODF_modes == 2){
				node_object.dirX.push_back(atof(str_vec[16].c_str()));
				node_object.dirY.push_back(atof(str_vec[17].c_str())); 
				node_object.dirZ.push_back(atof(str_vec[18].c_str()));
			}
			if(node_object.ODF_modes == 3){
				node_object.dirX.push_back(atof(str_vec[19].c_str()));
				node_object.dirY.push_back(atof(str_vec[20].c_str())); 
				node_object.dirZ.push_back(atof(str_vec[21].c_str()));
			}

			this->skeletonNodes.push_back(node_object);

			str_vec.clear();
		}
		nodePoints.close();
	}
	else{
		std::cout << " Could not open vessel nodes features file. Exiting now. " << std::endl;
		return;
	}

	if(this->skeletonNodes.empty()){
		std::cout << " Empty vessel node features file. Quitting. " << std::endl;
		return;
	}
	std::cout << "Vessel node features file read. " << this->skeletonNodes.size() << std::endl;
}

void VesselBasedNucleiFeatures::ReadNucleiLabelImage(const std::string file_path){

	Common::ReadImage3DUShort(file_path, this->nucLabelImage);
}

void VesselBasedNucleiFeatures::ReadSegmentationMask(const std::string file_path){

	Common::ReadImage3DUChar(file_path, this->vesselMaskImage);
}

void VesselBasedNucleiFeatures::SetInsideRegionToGlobal(void){

	this->insideRegion = this->vesselMaskImage->GetBufferedRegion();
}

void VesselBasedNucleiFeatures::ComputeSkeletonDistanceMap(void){

	std::cout << "Computing skeleton distance map... " << std::endl;

	SignedMaurerDistanceMapImageFilterType::Pointer MaurerFilter = SignedMaurerDistanceMapImageFilterType::New();
	MaurerFilter->SetInput(this->skeletonImage);
	MaurerFilter->SetSquaredDistance(false);
	MaurerFilter->SetUseImageSpacing(false);
	MaurerFilter->SetInsideIsPositive(false);
	MaurerFilter->Update();
	
	this->skeletonDistanceMap = MaurerFilter->GetOutput();
	
	// Write the skeleton distance map to disk
	/*std::string dist_file_name = this->data_folder_path;
	dist_file_name.append("_skeletonDistanceMap.mhd");
	Common::WriteImage3D(dist_file_name, this->skeletonDistanceMap);*/
}

void VesselBasedNucleiFeatures::ComputeNucleiDistanceMap(void){

	std::cout << "Computing nuclei distance map... " << std::endl;

	ThresholdFilterType::Pointer threshold_filter = ThresholdFilterType::New();
	threshold_filter->SetLowerThreshold(1);
	threshold_filter->SetInsideValue(255);
	threshold_filter->SetOutsideValue(0);
	threshold_filter->SetInput(this->nucLabelImage);
	threshold_filter->Update();

	this->nucBinaryImage = threshold_filter->GetOutput();

	SignedMaurerDistanceMapImageFilterType::Pointer MaurerFilter = SignedMaurerDistanceMapImageFilterType::New();
	MaurerFilter->SetInput(threshold_filter->GetOutput());
	MaurerFilter->SetSquaredDistance(false);
	MaurerFilter->SetUseImageSpacing(false);
	MaurerFilter->SetInsideIsPositive(false);
	MaurerFilter->Update();

	this->nucDistanceMap = MaurerFilter->GetOutput();
}

void VesselBasedNucleiFeatures::ComputeVesselFeaturesForNuclei(void){
	
	if(this->nucleiObjects.empty()){
		std::cout << "Nuclei objects vector is empty. Returning. " << std::endl;
		return;
	}

	std::cout << "Computing vessel features for nuclei... " << this->nucleiObjects.size() << std::endl;

	// Compute label geometry features - IF THIS WORKS, PUT IT IN THE ASTRO TRACER FOR NEUCLEI NEIGHBORHOOD USING MAJOR AXIS LENGTH
	LabelGeometryFilterType::Pointer label_geometry_filter = LabelGeometryFilterType::New();
	label_geometry_filter->SetInput(nucLabelImage);
	label_geometry_filter->Update();

	RenderImageType3D::IndexType starting_index_nuclei, end_index_nuclei;
	RenderImageType3D::SizeType sub_volume_size_nuclei;
	RenderImageType3D::RegionType sub_volume_region_nuclei, sub_volume_region_vessel;
	RenderImageType3D::Pointer sub_volume_nuclei, sub_volume_vessel;

	typedef itk::RegionOfInterestImageFilter<RenderImageType3D, RenderImageType3D> VolumeOfInterestFilterType_nuclei;
	typedef itk::RegionOfInterestImageFilter<RenderImageType3D, RenderImageType3D> VolumeOfInterestFilterType_vessel;

	for(int i = 0; i < this->nucleiObjects.size(); i++){
	
		itk::Index<3> nuc_centroid = this->nucleiObjects[i].intrinsicFeatures.centroid;

		// Distance to the closest vessel (centerline)
		this->nucleiObjects[i].associativeFeatures.distToVessel = this->skeletonDistanceMap->GetPixel(nuc_centroid);

		
		// Alignment with vessel

		// ROI proportional to nuclei scale, assuming spherical nuclei.
		//float sph_rad = std::pow((double)(0.23877 * this->nucleiObjects[i].intrinsicFeatures.volume), (double)0.333333);
		float sph_rad = 0.5 * label_geometry_filter->GetMajorAxisLength(this->nucLabelImage->GetPixel(nuc_centroid));
		float double_scale_nuclei = 1.5 * sph_rad;
		
		
		starting_index_nuclei[0] = nuc_centroid[0] - double_scale_nuclei; starting_index_nuclei[1] = nuc_centroid[1] - double_scale_nuclei; starting_index_nuclei[2] = nuc_centroid[2] - double_scale_nuclei;
		end_index_nuclei[0] = nuc_centroid[0] + double_scale_nuclei; end_index_nuclei[1] = nuc_centroid[1] + double_scale_nuclei; end_index_nuclei[2] = nuc_centroid[2] + double_scale_nuclei;

		LabelImageType3D::SizeType sz = this->nucLabelImage->GetBufferedRegion().GetSize();

		//std::cout << "Nuclei: Starting Indices:"<<starting_index_nuclei[0]<<" "<<starting_index_nuclei[1]<<" "<<starting_index_nuclei[2]<<" "<<std::endl;
		//std::cout << "Nuclei: End Indices:"<<end_index_nuclei[0]<<" "<<end_index_nuclei[1]<<" "<<end_index_nuclei[2]<<std::endl;

	    //std::cout << "Nuclei: "  << i << " Scale: " << double_scale_nuclei << std::endl;

		sub_volume_size_nuclei[0] = 2.0 * double_scale_nuclei; sub_volume_size_nuclei[1] = 2.0 * double_scale_nuclei; sub_volume_size_nuclei[2] = 2.0 * double_scale_nuclei;

		// Better boundary management, but gives vnl errors!!
		/*if(starting_index_nuclei[0] < 0){
			sub_volume_nuclei[0] = (long)sub_volume_nuclei[0] - (long)std::abs((int)starting_index_nuclei[0]);
			starting_index_nuclei[0] = 0;
		}
		if(starting_index_nuclei[1] < 0){
			sub_volume_nuclei[1] = sub_volume_nuclei[1] - (RenderImageType3D::PixelType)std::abs((int)starting_index_nuclei[1]);
			starting_index_nuclei[1] = 0;
		}
		if(starting_index_nuclei[2] < 0){
			sub_volume_nuclei[2] = sub_volume_nuclei[2] - (RenderImageType3D::PixelType)std::abs((int)starting_index_nuclei[2]);
			starting_index_nuclei[2] = 0;
		}
		if(end_index_nuclei[0] > sz[0]){
			sub_volume_nuclei[0] = sub_volume_nuclei[0] - (end_index_nuclei[0] - sz[0]);
			end_index_nuclei[0] = sz[0];
		}
		if(end_index_nuclei[1] > sz[1]){
			sub_volume_nuclei[1] = sub_volume_nuclei[1] - (end_index_nuclei[1] - sz[1]);
			end_index_nuclei[1] = sz[1];
		}
		if(end_index_nuclei[2] > sz[2]){
			sub_volume_nuclei[2] = sub_volume_nuclei[2] - (end_index_nuclei[2] - sz[2]);
			end_index_nuclei[2] = sz[2];
		}*/

		if ( (starting_index_nuclei[0] < 0) || (starting_index_nuclei[1] < 0) || (starting_index_nuclei[2] < 0) ||
			(end_index_nuclei[0] > (unsigned int)sz[0]) || (end_index_nuclei[1] > (unsigned int)sz[1]) ||
			(end_index_nuclei[2] > (unsigned int)sz[2]) )
			continue;

		//std::cout << "Nuclei: "  << i << " Scale: " << double_scale_nuclei << std::endl;

		double curr_distance, min_distance = 10000.0, max_distance = -10000.0, mean_distance = 0.0, var_distance = 0.0;
		int min_lin_idx = -1;
		std::vector<double> dist_vec;
		RenderImageType3D::IndexType curr_skeleton_idx, min_dist_idx, max_dist_idx;

		for(int j = 0; j < this->skeletonNodes.size(); j++){
		
			curr_skeleton_idx[0] = this->skeletonNodes[j].x; curr_skeleton_idx[1] = this->skeletonNodes[j].y; curr_skeleton_idx[2] = this->skeletonNodes[j].z;

			int offset = 1; //1;
			if(curr_skeleton_idx[0] < starting_index_nuclei[0]+offset || curr_skeleton_idx[1] < starting_index_nuclei[1]+offset || curr_skeleton_idx[2] < starting_index_nuclei[2]+offset ||
				curr_skeleton_idx[0] > end_index_nuclei[0]-offset || curr_skeleton_idx[1] > end_index_nuclei[1]-offset || curr_skeleton_idx[2] > end_index_nuclei[2]-offset)
				continue;

			curr_distance = this->nucDistanceMap->GetPixel(curr_skeleton_idx);

			//std::cout << "Vessel point: " << j << " Dist: " << curr_distance << std::endl;
			
			dist_vec.push_back(curr_distance);

			if(curr_distance < min_distance){
				
				min_distance = curr_distance;
				min_dist_idx = curr_skeleton_idx;
				min_lin_idx = j;
			}

			if(curr_distance > max_distance){

				max_distance = curr_distance;
				max_dist_idx = curr_skeleton_idx;
			}
		}

			
		//	CHECK IF ANY NEIGHBORHOOD POINT WAS DETECTED
		if(!dist_vec.empty()){

			//std::vector<std::vector<double> > vessel_dir_all; //(3, std::vector<double>(3, 0.0));
			vnl_vector<double> vessel_dir(3, 0.0);
		    double max_abs_cosine = -2.0, ang = 0.0, abs_cos_angle = 0.0;
			
			LabelGeometryFilterType::MatrixType nuc_dir = label_geometry_filter->GetEigenvectors(this->nucLabelImage->GetPixel(nuc_centroid));
			LabelGeometryFilterType::VectorType eigen_vals = label_geometry_filter->GetEigenvalues(this->nucLabelImage->GetPixel(nuc_centroid));
			//nuc_dir.normalize_columns(); 
			
			vnl_vector<double> eigen_vals_vnl(eigen_vals.size(), 0.0);
			for(int l = 0; l < eigen_vals.size(); l++)
				eigen_vals_vnl[l] = eigen_vals[l];
			
			int arg_max_ev = eigen_vals_vnl.arg_max();
			vnl_vector<double> largest_axis_dir = nuc_dir.get_column(arg_max_ev);
			vnl_vector<double> abs_cos_vec(3, -1.0), ang_vec(3, 0.0);

			for(int k = 0; k < this->skeletonNodes[min_lin_idx].ODF_modes; k++){

				vessel_dir[0] = this->skeletonNodes[min_lin_idx].dirX[k];
				vessel_dir[1] = this->skeletonNodes[min_lin_idx].dirY[k];
				vessel_dir[2] = this->skeletonNodes[min_lin_idx].dirZ[k];
				//vessel_dir.normalize();
				

				ang = angle(vessel_dir, largest_axis_dir);
				abs_cos_angle = vcl_abs(vcl_cos(ang));
				
				ang_vec[k] = ang;
				abs_cos_vec[k] = abs_cos_angle; 
				
				//if(abs_cos_angle > max_abs_cosine)
				//	max_abs_cosine = abs_cos_angle;
				
			}

			int arg_max_abs_cos = abs_cos_vec.arg_max();
			double align_with_vessel = ang_vec[arg_max_abs_cos];
			

			double mean_distance = 0.0, variance_distance = 0.0;
			mean_distance = std::accumulate(dist_vec.begin(), dist_vec.end(), 0.0) / (double)dist_vec.size();
			
			for(int k = 0; k < dist_vec.size(); k++)
				variance_distance = variance_distance + std::pow(dist_vec[k] - mean_distance, 2);

			variance_distance = variance_distance / (double)dist_vec.size();
			
			//if(variance_distance < 0.001)
			//	std::cout << mean_distance << ", " << variance_distance << std::endl;

			this->nucleiObjects[i].associativeFeatures.alignmentWithVessel = align_with_vessel;
			this->nucleiObjects[i].associativeFeatures.nVesselPoints = dist_vec.size();
			this->nucleiObjects[i].associativeFeatures.meanVesselDist = mean_distance;
			this->nucleiObjects[i].associativeFeatures.varVesselDist = variance_distance;
		
		}
		else{
			this->nucleiObjects[i].associativeFeatures.alignmentWithVessel = -1.0;
			this->nucleiObjects[i].associativeFeatures.nVesselPoints = 0;
			this->nucleiObjects[i].associativeFeatures.meanVesselDist = 10000;
			this->nucleiObjects[i].associativeFeatures.varVesselDist = 10000;
		}
		

		
		// Overlap with vessel

		
		sub_volume_region_nuclei.SetIndex(starting_index_nuclei);
		sub_volume_region_nuclei.SetSize(sub_volume_size_nuclei);

		sub_volume_region_vessel.SetIndex(starting_index_nuclei);
		sub_volume_region_vessel.SetSize(sub_volume_size_nuclei);
		
		//std::cout << starting_index_nuclei << ", " << end_index_nuclei << std::endl;
		
		VolumeOfInterestFilterType_nuclei::Pointer sub_volume_filter_nuclei = VolumeOfInterestFilterType_nuclei::New();
		sub_volume_filter_nuclei->SetInput(this->nucBinaryImage);
		sub_volume_filter_nuclei->SetRegionOfInterest(sub_volume_region_nuclei);
		sub_volume_filter_nuclei->Update();
		sub_volume_nuclei = sub_volume_filter_nuclei->GetOutput();
		
		//std::cout << "Nuclei: "  << i << " Scale: " << double_scale_nuclei << std::endl;	

		VolumeOfInterestFilterType_vessel::Pointer sub_volume_filter_vessel = VolumeOfInterestFilterType_vessel::New();
		sub_volume_filter_vessel->SetInput(this->vesselMaskImage);
		sub_volume_filter_vessel->SetRegionOfInterest(sub_volume_region_vessel);
		sub_volume_filter_vessel->Update();
		sub_volume_vessel = sub_volume_filter_vessel->GetOutput();

		LabelOverlapFilterType::Pointer label_overlap_filter = LabelOverlapFilterType::New();
		label_overlap_filter->SetSourceImage(sub_volume_vessel);
		label_overlap_filter->SetTargetImage(sub_volume_nuclei);
		label_overlap_filter->Update();

		this->nucleiObjects[i].associativeFeatures.overlapWithVessel = label_overlap_filter->GetTotalOverlap();

		//std::cout << this->nucleiObjects[i].associativeFeatures.overlapWithVessel << std::endl;
	
	}

	std::cout << "Done with computing vessel features for nuclei. " << std::endl;
}

void VesselBasedNucleiFeatures::WriteNucleiFeatureTable(void){

	if(this->nucleiObjects.empty()){
		std::cout << "Nuclei objects container is empty. Returning. " << std::endl;
		return;
	}

	std::cout << "Writing final nuclei features file... " << std::endl;

	std::string outputFname = this->data_folder_path;
	outputFname.append("_nucFeaturesVessel.txt");

	std::ofstream nuclei_feature_vector;
	nuclei_feature_vector.open(outputFname.c_str(), std::ios::out);
	//std::cout << "After feature_vector.open"<<std::endl;
	
	unsigned short IDIndex = 0;//ID index
	nuclei_feature_vector << "ID" << '\t' << "x" << '\t' << "y" << '\t' << "z" << '\t' << "volume" << '\t' << "sum_int" << '\t' << "mean_int" << '\t';	
	nuclei_feature_vector << "var_int" << '\t' << "eccentricity" << '\t' << "elongation" << '\t' << "mean_surf_gradient" << '\t' << "radius_variation" << '\t'; 
	nuclei_feature_vector << "shape_measure" << '\t' << "energy" << '\t' << "entropy" << '\t' << "inverse_diff_moment" << '\t' << "inertia" << '\t';
	nuclei_feature_vector << "cluster_shade" << '\t' << "cluster_prominence" << '\t' << "Astro_TOTAL" << '\t' << "Astro_AVG" << '\t' << "Astro_SURR" << '\t';
	nuclei_feature_vector << "Micro_TOTAL" << '\t' << "Micro_AVG" << '\t' << "Micro_SURR" << '\t' << "Neuro_TOTAL" << '\t' << "Neuro_AVG" << '\t';
	nuclei_feature_vector << "Neuro_SURR" << '\t' <<  "Vessel_TOTAL" << '\t' << "Vessel_AVG" << '\t' << "Vessel_SURR" << '\t';
	nuclei_feature_vector << "min_root_dist" << '\t' << "max_root_dist" << '\t' << "mean_root_dist" << '\t' << "var_root_dist" << '\t' << "n_roots" << '\t';
	nuclei_feature_vector << "dist_to_vessel"  << '\t' << "mean_vessel_dist" << '\t' << "var_vessel_dist" << '\t' << "n_vessel_points" << '\t';
	nuclei_feature_vector << "align_with_vessel" << '\t' << "overlap_with_vessel" << '\t';
	nuclei_feature_vector << std::endl;

	for(size_t i = 0; i < this->nucleiObjects.size(); i++){

		NucleiObject_VT nuc = this->nucleiObjects[i];
		
		//if(nuc.associativeFeatures.nRoots != 0){
			nuclei_feature_vector << nuc.intrinsicFeatures.ID << '\t' << nuc.intrinsicFeatures.centroid[0] << '\t' << nuc.intrinsicFeatures.centroid[1] << '\t' << nuc.intrinsicFeatures.centroid[2] << '\t';
			nuclei_feature_vector << nuc.intrinsicFeatures.volume << '\t' << nuc.intrinsicFeatures.integratedIntensity << '\t' << nuc.intrinsicFeatures.meanIntensity << '\t' << nuc.intrinsicFeatures.varianceIntensity << '\t';
			nuclei_feature_vector << nuc.intrinsicFeatures.eccentricity << '\t' << nuc.intrinsicFeatures.elongation << '\t' << nuc.intrinsicFeatures.meanSurfaceGradient << '\t' << nuc.intrinsicFeatures.radiusVariation << '\t';
			nuclei_feature_vector << nuc.intrinsicFeatures.shapeMeasure << '\t' << nuc.intrinsicFeatures.energy << '\t' << nuc.intrinsicFeatures.entropy << '\t' << nuc.intrinsicFeatures.inverseDiffMoment << '\t';
			nuclei_feature_vector << nuc.intrinsicFeatures.inertia << '\t' << nuc.intrinsicFeatures.clusterShade << '\t' << nuc.intrinsicFeatures.clusterProminence << '\t'; 
			nuclei_feature_vector << nuc.associativeFeatures.astro_total << '\t' << nuc.associativeFeatures.astro_avg << '\t' << nuc.associativeFeatures.astro_surr << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.micro_total << '\t' << nuc.associativeFeatures.micro_avg << '\t' << nuc.associativeFeatures.micro_surr << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.neuro_total << '\t' << nuc.associativeFeatures.neuro_avg << '\t' << nuc.associativeFeatures.neuro_surr << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.vessel_total << '\t' << nuc.associativeFeatures.vessel_avg << '\t' << nuc.associativeFeatures.vessel_surr << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.minRootDist << '\t' << nuc.associativeFeatures.maxRootDist << '\t' << nuc.associativeFeatures.meanRootDist << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.varRootDist << '\t' << nuc.associativeFeatures.nRoots << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.distToVessel << '\t' << nuc.associativeFeatures.meanVesselDist << '\t' << nuc.associativeFeatures.varVesselDist << '\t';
			nuclei_feature_vector << nuc.associativeFeatures.nVesselPoints << '\t' << nuc.associativeFeatures.alignmentWithVessel << '\t' << nuc.associativeFeatures.overlapWithVessel << '\t';
			nuclei_feature_vector << std::endl;
		//}
	}
	nuclei_feature_vector.close();
	std::cout << "Done with writing nuclei feature vector file: " << outputFname << std::endl;

}