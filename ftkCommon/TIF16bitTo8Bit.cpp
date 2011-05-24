#include "itkImage.h"
#include "itkImageFileWriter.h"
#include "itkImageFileReader.h"
#include "itkRescaleIntensityImageFilter.h"

int main(int argc, char *argv[])
{
	typedef unsigned short InputPixelType;
	typedef unsigned char OutputPixelType;
	
	const unsigned Dimension = 3;

	typedef itk::Image<InputPixelType, Dimension> InputImageType;
	typedef itk::Image<OutputPixelType, Dimension> OutputImageType;

	typedef itk::ImageFileReader<InputImageType> InputReaderType;
	typedef itk::ImageFileWriter<OutputImageType> OutputWriterType;

	InputReaderType::Pointer reader = InputReaderType::New();
	OutputWriterType::Pointer writer = OutputWriterType::New();

	std::string outputFileName = "8bit";
	const char* inputFilename = argv[1];

	reader->SetFileName(inputFilename);
	reader->Update();

	typedef itk::RescaleIntensityImageFilter<InputImageType, OutputImageType> RescaleFilterType;
	RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();

	rescaleFilter->SetInput(reader->GetOutput());
	rescaleFilter->SetOutputMinimum(0);
	rescaleFilter->SetOutputMaximum(255);
	rescaleFilter->Update();

	writer->SetInput(rescaleFilter->GetOutput());
	
	outputFileName.append(inputFilename);
	
	writer->SetFileName(outputFileName);
	writer->Update();	
}