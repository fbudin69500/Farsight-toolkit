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
//---Xiao liang  2009.12.5-------//

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#include <iostream>

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

#include "distTransform.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

#define DATATYPEIN unsigned char
#define DATATYPEOUT unsigned char

struct  VoxelPosition
{
  float x;
  float y;
  float z;
}; 


int main(int argc, char *argv[])
{
   int sizeX, sizeY, sizeZ;

  typedef unsigned char OutputPixelType;
  typedef itk::Image< OutputPixelType, 3 > OutputImageType;
  //check that the user specified the right number of command line arguments
  if(argc < 3)
    {
    cerr << argv[0] << " <input binary image file>  <Disdance output  file>" << endl;
    cerr << argv[0] << " <input binary image raw input file> <sizeX> <sizeY> <sizeZ>  <Disdance output file>"
         << endl;
    return EXIT_FAILURE;
    }

  //check if the input image is .raw
  bool rawInput = false;
  string inputFileName = argv[1];
  const char *DisdanceFileName;

  if(inputFileName.rfind(".raw") != string::npos)
    {
    //if so, the user is also required to pass in image dimensions
    if(argc < 6)
      {
      cerr << "Usage: <input binary image raw input file> <sizeX> <sizeY> <sizeZ> <Disdance output file>" << endl;
      return EXIT_FAILURE;
      }
    rawInput = true;
    sizeX = atoi(argv[2]);
    sizeY = atoi(argv[3]);
    sizeZ = atoi(argv[4]);
    DisdanceFileName = argv[5];
    }
  else
    {
    DisdanceFileName = argv[2];
    }
  
  DATATYPEIN *volin;
 
  FILE *neuronSegfile = 0; // 
  FILE *outfile;

  int sizeExpand = 0;

  //make sure we can write to the output file
  if((outfile=fopen(DisdanceFileName, "wb")) == NULL)
    {
    cerr << "Output file open error!" << endl;
    return EXIT_FAILURE;
    }
  //initialize the input image
  if(rawInput)
    {
    if((neuronSegfile=fopen(inputFileName.c_str(), "rb"))==NULL)
      {
      cout << "Input file open error!" << endl;
      return EXIT_FAILURE;
      }

    volin = (DATATYPEIN*)malloc(sizeX*sizeY*(sizeZ+sizeExpand*2)*sizeof(DATATYPEIN));

    if (fread(volin, sizeof(DATATYPEIN), sizeX*sizeY*sizeZ, neuronSegfile) < (unsigned int)(sizeX*sizeY*sizeZ))
      {
      cerr << "File size is not the same as volume size" << endl;
      return EXIT_FAILURE;
      }
    }
  else
    {
    //use ITK to read all non-raw images
    typedef unsigned char     PixelType;
    const   unsigned int      Dimension = 3;
    typedef itk::Image< PixelType, Dimension >    ImageType;
    typedef itk::ImageFileReader< ImageType >  ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    ImageType::Pointer inputImage = reader->GetOutput();
    reader->SetFileName( inputFileName.c_str()  );
    try 
      { 
      reader->Update(); 
      } 
    catch( itk::ExceptionObject & err ) 
      { 
      cerr << "ExceptionObject caught!" << endl; 
      cerr << err << endl; 
      return EXIT_FAILURE;
      } 
    ImageType::RegionType region = inputImage->GetBufferedRegion();
    ImageType::SizeType  size = region.GetSize();
    cout << "input image size: " << size << endl;
    sizeX = size[0];
    sizeY = size[1];
    sizeZ = size[2];
    
    //manually copy the values from inputImage to volin, which we will use for
    //the rest of the MDL process
    itk::ImageRegionIterator< ImageType >
      itr( inputImage, inputImage->GetBufferedRegion() );
    itr.GoToBegin();
    long int idx = 0;
    volin = (DATATYPEIN*)malloc(sizeX*sizeY*(sizeZ+sizeExpand*2)*sizeof(DATATYPEIN));
    while( ! itr.IsAtEnd() )
      {
      volin[idx] = itr.Get();
      ++itr;
      ++idx;
      }
    }

  //allocate memory for the output image
  // test of dist trans

  printf("I come to distance transform !");
  distTransform(volin, sizeX, sizeY, sizeZ);
  fwrite(volin, sizeX*sizeY*sizeZ, sizeof(DATATYPEOUT), outfile);
/*
  typedef  itk::ImageFileWriter<  OutputImageType  > WriterType;
  WriterType::Pointer writer = WriterType::New();
  Const IteratorType in(inputImage,inputImage->GetRequestRegion());
  IteratorType out(outputImage,inputImage->GetRequestRegion()) ;

  long idx = 0;
  while(! itr.IsAtEnd())
  {	   ++idx;
	  out.set(volin[idx]);
  }

  //writer->SetInput(volin);
  writer->SetFileName("DistanceField.tif");   
  writer->Update();
*/
  if (rawInput)
	  fclose(neuronSegfile);
  fclose(outfile);

  free(volin);  
  volin=NULL;
  cout << "DT is Done" << endl;
  return 0;
}