/*=========================================================================
Copyright 2009 Rensselaer Polytechnic Institute
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
=========================================================================*/

/*=========================================================================
  Program:   Open Snake Tracing System
  Autohr:    Yu Wang
  Email: wangy15@rpi.edu
  Language:  C++
  Date:      $Date:  $
  Version:   $Revision: 0.00 $
=========================================================================*/

#ifndef IMAGEOPERATION_H
#define IMAGEOPERATION_H

#include "time.h"

#include "itkBinaryThresholdImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
//#include "itkImageSeriesReader.h"
#include "itkSubtractImageFilter.h"
#include "itkNeighborhoodIterator.h"
#include "itkConstNeighborhoodIterator.h"
#include "itkImageIterator.h"
#include "itkImageIteratorWithIndex.h"

#include "itkListSample.h"
#include "itkKdTree.h"
#include "itkKdTreeGenerator.h"

#include "Filters/itkBinaryThinningImageFilter3D.h"
#include "itkMedianImageFilter.h"

#include "itkImageSliceConstIteratorWithIndex.h"
#include "itkImageSliceIteratorWithIndex.h"
#include "itkImageLinearIteratorWithIndex.h"

#include "itkVotingBinaryHoleFillingImageFilter.h"

#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "Filters/itkLabelGeometryImageFilter.h"

//#include "itkImageToVTKImageFilter.h" 

#include "itkLabelStatisticsImageFilter.h"

//These two are used in background class
#include "itkImageDuplicator.h"
#include "itkShrinkImageFilter.h" 

#include "Vesselness/itkAnisotropicDiffusionVesselEnhancementImageFilter.h"
#include "Vesselness/itkMultiScaleHessianSmoothed3DToVesselnessMeasureImageFilter.h"
#include "Vesselness/itkVesselEnhancingDiffusion3DImageFilter.h"
#include "itkHessian3DToVesselnessMeasureImageFilter.h"
#include "Vesselness/itkMultiScaleHessianBasedMeasureImageFilter.h"

#include "itkGradientVectorFlowImageFilter.h"
#include "itkGradientImageFilter.h"
#include "itkCovariantVector.h"
#include "itkRecursiveGaussianImageFilter.h"
#include "itkGradientRecursiveGaussianImageFilter.h"
#include "itkGradientAnisotropicDiffusionImageFilter.h"
//#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
//#include "itkCurvatureFlowImageFilter.h"
//#include "itkGradientAnisotropicDiffusionImageFilter.h"
#include "itkLaplacianRecursiveGaussianImageFilter.h"

#include "itkExtractImageFilter.h"
#include "itkVectorLinearInterpolateImageFunction.h"
#include "itkInterpolateImageFunction.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkImageFunction.h"


#include "itkPermuteAxesImageFilter.h"

#include "GCv2p2/GCoptimization.h"
#include "GCv2p2/graph.h"

#include "PointOperation.h"

#include "itkRescaleIntensityImageFilter.h"
#include "itkMaximumProjectionImageFilter.h"

#include "itkImageSeriesReader.h"
#include "itkTIFFImageIO.h"

#include "itkFastMarchingImageFilter.h"
#include "itkGeodesicActiveContourLevelSetImageFilter.h"
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"

#include "itkNumericTraits.h"
#include "itkPolyLineParametricPath.h"

#include "itkDanielssonDistanceMapImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryBallStructuringElement.h"

//#include "itkArrivalFunctionToPathFilter.h"
//#include "itkSpeedFunctionToPathFilter.h"
//#include "itkPathIterator.h"
//#include "itkGradientDescentOptimizer.h"
//#include "itkRegularStepGradientDescentOptimizer.h"

#include "itkVectorContainer.h"

#include <vnl/vnl_matrix.h>
#include <vnl/vnl_vector.h>

#define PG_MAX(a,b)  (a>b? a: b)
#define PG_MIN(a,b)  (a<b? a: b)

typedef unsigned char  PGbyte;
typedef          char  PGchar;
typedef unsigned short PGushort;
typedef          short PGshort; 
typedef unsigned int   PGuint;
typedef          int   PGint;
typedef float          PGfloat;
typedef double         PGdouble;
typedef void           PGvoid;

typedef itk::Image<unsigned char, 3> IOImageType;
typedef IOImageType::Pointer IOImagePointer;

typedef itk::Image<unsigned short, 3> IOImageType1;
typedef IOImageType1::Pointer IOImagePointer1;

typedef itk::RGBPixel< unsigned char>  IORGBPixelType;
typedef itk::Image<IORGBPixelType, 3> IORGBImageType;
typedef IORGBImageType::Pointer IORGBImagePointer;

typedef itk::RGBPixel< float >  RGBPixelType;
typedef itk::Image<RGBPixelType, 3> RGBImageType;
typedef RGBImageType::Pointer RGBImagePointer;

typedef itk::Image< RGBPixelType, 2 > RGBImageType2D;
typedef RGBImageType2D::Pointer RGBImagePointer2D;

typedef itk::Image<signed int, 3> ImageType;
typedef ImageType::Pointer ImagePointer;

typedef itk::Image<signed int, 2 > ImageType2D;
typedef ImageType2D::Pointer ImagePointer2D;

typedef itk::Image<float, 3> ProbImageType;
typedef ProbImageType::Pointer ProbImagePointer;

typedef itk::Image<float, 2> ProbImageType2D;
typedef ProbImageType2D::Pointer ProbImagePointer2D;

typedef itk::Image<unsigned short int, 3> LabelImageType;
typedef LabelImageType::Pointer LabelImagePointer;

typedef itk::Image<unsigned short int, 2> LabelImageType2D;
typedef LabelImageType2D::Pointer LabelImagePointer2D;

typedef itk::CovariantVector< float, 3 >  GradientPixelType;
typedef itk::Image< GradientPixelType, 3 > GradientImageType;
typedef GradientImageType::Pointer GradientImagePointer;

typedef itk::Vector< float, 3 > SeedType;
typedef itk::Statistics::ListSample< SeedType > SampleType;
typedef SampleType::Pointer SamplePointer;


//GPU functions for vesselness and gvf
//extern "C"
//void do_eigen_decomp(float *Dxx, float *Dxy, float *Dxz,
//                     float *Dyy, float *Dyz, float *Dzz, int N, float *vesselness, float *vx1, float *vy1, float *vz1);
//extern "C"
//void gvf_iteration(float *u, float *v, float *o, float *f, float mu, int ITER, int YN, int XN, int ZN);

//extern "C"
//void gradient(float *u, float *v, float *o, float *f, int YN, int XN, int ZN);

//extern "C"
//void Hessian(float *Dxx, float *Dxy, float *Dxz, float *Dyy, float *Dyz, float *Dzz, float *u, float *v, float *o, int YN, int XN, int ZN);

//functions for eig3volume
static void tred2(double V[3][3], double d[3], double e[3]);
static void tql2(double V[3][3], double d[3], double e[3]);
void eigen_decomposition(double A[3][3], double V[3][3], double d[3]);
double MAX(double a, double b);
static double hypot2(double x, double y);
double absd(double val);

//functions for gvfc
PGfloat ***pgDmatrix(int M, int N, int K);
PGvoid pgFreeDmatrix(PGfloat ***Image, int M, int N, int K);

void GVFC(int XN, int YN, int ZN, float *f, float *ou, float *ov, float *oo, 
	  float mu, int ITER);
void GVFC_CUDA(int XN, int YN, int ZN, float *f, float *ou, float *ov, float *oo, 
	  float mu, int ITER);



ProbImagePointer extract_one_component(int index, GradientImagePointer IG);
//PointList3D extract_local_maximum(ProbImagePointer V_saliency, int rad, double th, ImagePointer BW, bool local_maxima);
class VectorPixelAccessor;


class ImageOperation
{
	
public:

	ImageOperation(void);

	int SM;
	int SN;
	int SZ;
	bool mask_set;
	bool display_set; 
	float *u, *v, *o;

	float u1,u2,sigma1,sigma2; //intensity models
	std::vector<float> I_cu;

	//IOImagePointer IIO;

	ImagePointer I;

	//RGBImagePointer2D IRGB;
	ImagePointer IDisplay;

	//ImagePointer2D I2D;
	ImagePointer IMask;//Soma Mask;
	ImagePointer VBW;//Binary Image
	//ImagePointer DBW;//Depth Image
	ProbImagePointer IGMag;
	//ProbImagePointer IDist;
	ImagePointer ISeg;//Segmentation Image/Reconstruction
	
	LabelImagePointer IL;
    LabelImagePointer IL_Tracing;

	LabelImagePointer ISoma;
	LabelImagePointer IVoronoi;
	int num_soma;
    PointList3D Centroid;

	ProbImagePointer IVessel;
	ProbImagePointer IBranch;

	ProbImagePointer Dxx;
	ProbImagePointer Dxy;
	ProbImagePointer Dxz;
	ProbImagePointer Dyy;
	ProbImagePointer Dyz;
	ProbImagePointer Dzz;

    GradientImagePointer IGVF;
	GradientImagePointer IG;
	
    GradientImagePointer V1;

    PointList3D SeedPt;


	int current_seed_idx;
    vnl_vector<int> visit_label;

    //intensity models for 4-D region based open-curve snake
	void ImComputeBackgroundModel();
	void ImComputeForegroundModel(PointList3D Cu, std::vector<float> Ru);

    void ConvertReadImage();
    void ConvertWriteImage();
	//void ConvertWriteLabelImage();
	void ImRemoveSlice(int in);
    void ImBW(int Threshold);
    void ImOrdFilt2();
    void ImHoleFill2D();
    void ImHoleFill3D();
	IOImagePointer ImBkSub(IOImagePointer In);
	void ImSeriesReadWrite(std::vector< std::string > filenames, const char *save_name, int shrink_factor, bool sixteen_bit);
	void ImRead(const char *filename);
	void ImRead_NoSmooth(const char *filename, int in);
	void ImDisplayRead(const char *filename, int shrink_factor);
    void ImWrite(const char *filename, int image_id);
	void ImWrite_Soma(const char *filename);
	ImagePointer ImCopy();
	void ImMasking(PointList3D contour); //manual masking
	void ImMasking(const char *filename, int shrink_factor);
	void ImMasking(int shrink_factor);
	IOImagePointer ImInvert(IOImagePointer In);
	void ImSub(ImagePointer BG);
	void ImShrink(int shrink_factor);
	IOImagePointer ImShrink(IOImagePointer In, int shrink_factor);
    void ImThinning(bool clean_skeleton, int min_length);
	void ImMedian(int radius);

	void ImGraphCut(double th, bool hole_filling, bool clean_skeleton, int min_length, int segmentation_method);
	//void ImGraphCut1(double th, bool hole_filling, bool clean_skeleton, int min_length, int segmentation_method);
	void ImFastMarchingAnimation(int threshold);

	void clear_IMask();
	void ImFastMarching_Soma(PointList3D seg_seeds);//segment single soma
	void ImFastMarchingI(PointList3D seg_seeds);
	void ImFastMarchingI_New(PointList3D seg_seeds); //automatic threshold selecting
    void ImFastMarchingII(PointList3D seg_seeds, int idx);
    void ImLevelSet(int th, bool hole_filling, bool clean_skeleton, int min_length, int seed_radius);

    void ImCoding(PointList3D Cu, int id, bool tracing);
    void ImRefresh_LabelImage();
	void ImRefresh_TracingImage();
    void ImSmoothing(int smoothing_scale);


	void BinaryToDepth();

	void MinimalPathCorrection(PointList3D path);

    ProbImagePointer ImGaussian(ProbImagePointer I_In, int sigma);
	ImagePointer ImGaussian(ImagePointer I_In, int sigma);
	IOImagePointer ImGaussian_XY(IOImagePointer I_In, int sigma);
	void seed_centroid();

	ImagePointer ImRescale(ProbImagePointer IR);
    ImagePointer ImRescale(ImagePointer IR);
	ImagePointer ImRescale(LabelImagePointer IR);

	ImagePointer2D ImMaxProjection(ImagePointer IInput);
	ProbImagePointer2D ImMaxProjection(ProbImagePointer IInput);
    LabelImagePointer2D ImMaxProjection(LabelImagePointer IInput);
	RGBImagePointer2D ImMaxProjection(RGBImagePointer IInput);
	RGBImagePointer2D ImMaxProjection1(ImagePointer IInput);

	void ComputeMultiVesselness(double sigma_min, double sigma_max, int sigma_step);
	void ComputeGVFVesselness();
	void ComputeGVFVesselness_CUDA();
	void ComputeGVFVesselness_CUDA_New(float *vesselness, float *vx1, float *vy1, float *vz1);
    void ComputeBranchiness();

	void VesselEnhancing(int sigma_min, int sigma_max, int sigma_step);
    void normalizeGVF();
    void computeGVF_C(int noise_level, int num_iteration, bool cuda, int smooth_scale);
	void computeGVF(int mu, int ITER, int smoothing_scale);

	void SeedDetection(float th, int detection_method, int radius);
	void SeedAdjustment(int iter_num);
	bool SeedSparsify(SamplePointer seeds_candidate, SeedType query_point, int radius);

    ImagePointer2D extract_one_slice_yz(ImagePointer I_input, int index);
    ProbImagePointer2D extract_one_slice_yz(ProbImagePointer I_input, int index);

	ImagePointer2D extract_one_slice_xz(ImagePointer I_input, int index);
    ProbImagePointer2D extract_one_slice_xz(ProbImagePointer I_input, int index);

	ImagePointer2D extract_one_slice(ImagePointer I_input, int index);
    ProbImagePointer2D extract_one_slice(ProbImagePointer I_input, int index);
	 
    //void find_head_tail();
    //void find_center();
};

//accessor for extracting one component of the vector image
class VectorPixelAccessor  
{
public:
  typedef itk::CovariantVector<float,3>   InternalType;
  typedef                      float      ExternalType;

  void operator=( const VectorPixelAccessor & vpa )
    {
      m_Index = vpa.m_Index;
    }
  ExternalType Get( const InternalType & input ) const 
    {
    return static_cast<ExternalType>( input[ m_Index ] );
    }
  void SetIndex( unsigned int index )
    {
    m_Index = index;
    }
private:
  unsigned int m_Index;
};

#endif