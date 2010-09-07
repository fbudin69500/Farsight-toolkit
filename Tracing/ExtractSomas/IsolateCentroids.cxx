//image read/type classes
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

//label object classes
#include "itkShapeLabelObject.h"
#include "itkShapeLabelMapFilter.h" 
#include "itkBinaryImageToShapeLabelMapFilter.h"

//for min/max limits
#include <float.h>
#include <limits.h>

int main( int argc, char ** argv )
{
  // Verify the number of parameters in the command line
  if( argc < 2 )
    {
    std::cerr << "Usage: " << std::endl;
    std::cerr << argv[0] << " binarySomaImageInput " << std::endl;
    return EXIT_FAILURE;
    }

  typedef unsigned short      InputPixelType;
  const   unsigned int        Dimension = 3;
  typedef itk::Image< InputPixelType, Dimension >    ImageType;

  typedef itk::ImageFileReader< ImageType >  ReaderType;
  typedef itk::ImageFileWriter< ImageType >  WriterType;
  
  typedef unsigned long LabelType;
  typedef itk::ShapeLabelObject< LabelType, Dimension > LabelObjectType;
  typedef itk::LabelMap< LabelObjectType > LabelMapType;

  ReaderType::Pointer reader = ReaderType::New();
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName("centroids.tif");

  const char * inputFilename  = argv[1];
  reader->SetFileName( inputFilename  );

  //we use a BinaryImageToShapeLabelMapFilter to convert the binary
  //input image into a collection of objects
  typedef itk::BinaryImageToShapeLabelMapFilter< ImageType, LabelMapType >
    ConverterType;
  ConverterType::Pointer converter = ConverterType::New();
  converter->SetInputForegroundValue(255);
  converter->SetInput(reader->GetOutput());
  LabelMapType::Pointer Somas = converter->GetOutput();
  converter->Update();
  unsigned int numSomas = Somas->GetNumberOfLabelObjects();

  std::cout << "I count " << numSomas << " distinct somas." << std::endl; 
  
  //make a new image that's all black except for the centroids of the somas
  //we'll use this to generate a voronoi partition of the cells
  ImageType::Pointer centroidImage = ImageType::New();
  ImageType::IndexType start;
  start[0] =   0;
  start[1] =   0;
  start[2] =   0;
  ImageType::RegionType region;
  region.SetSize( reader->GetOutput()->GetLargestPossibleRegion().GetSize() );
  region.SetIndex( start );
  centroidImage->SetRegions( region );
  centroidImage->Allocate();
  centroidImage->FillBuffer(0);

  double minElongation = DBL_MAX;
  double maxElongation = DBL_MIN;
  double avgElongation = 0.0;
  double minFlatness = DBL_MAX;
  double maxFlatness = DBL_MIN;
  double avgFlatness = 0.0;
  double minRadius = DBL_MAX;
  double maxRadius = DBL_MIN;
  double avgRadius = 0.0;
  unsigned long minSize = ULONG_MAX;
  unsigned long maxSize = 0;
  unsigned long avgSize = 0;
  for(unsigned int label=1; label<= numSomas; ++label)
    {
    const LabelObjectType * labelObject = Somas->GetLabelObject(label);
    
    ImageType::IndexType pixelIndex;
    reader->GetOutput()->TransformPhysicalPointToIndex( labelObject->GetCentroid(), pixelIndex );
    centroidImage->SetPixel(pixelIndex, 255);

    double elongation = labelObject->GetBinaryElongation();
    if(elongation < minElongation)
      {
      minElongation = elongation;
      }
    if(elongation > maxElongation)
      {
      maxElongation = elongation;
      }

    double flatness = labelObject->GetBinaryFlatness();
    if(flatness < minFlatness)
      {
      minFlatness = flatness;
      }
    if(flatness > maxFlatness)
      {
      maxFlatness = flatness;
      }

    double radius = labelObject->GetEquivalentRadius();
    if(radius < minRadius)
      {
      minRadius = radius;
      }
    if(radius > maxRadius)
      {
      maxRadius = radius;
      }

    unsigned long size = labelObject->GetSize();
    if(size < minSize)
      {
      minSize = size;
      }
    if(size > maxSize)
      {
      maxSize = size;
      }

    avgElongation += elongation;
    avgFlatness += flatness;
    avgRadius += radius;
    avgSize += size;
    }
  avgElongation /= numSomas;
  avgFlatness /= numSomas;
  avgRadius /= numSomas;
  avgSize /= numSomas;
 
  std::cout << "\t\t(min\tavg\tmax)" << std::endl;
  std::cout << "Elongation\t" << minElongation << "\t" << avgElongation << "\t"
            << maxElongation << std::endl;
  std::cout << "Flatness\t" << minFlatness << "\t" << avgFlatness << "\t"
            << maxFlatness << std::endl;
  std::cout << "Radius\t\t" << minRadius << "\t" << avgRadius << "\t"
            << maxRadius << std::endl;
  std::cout << "Size\t\t" << minSize << "\t" << avgSize << "\t" << maxSize
            << std::endl;
  
  writer->SetInput(centroidImage);
  writer->Update();
  return EXIT_SUCCESS; 
}