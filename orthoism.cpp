//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//     Author: Oscar Kramer, oscarkramer@yahoo.com.com
//
//**************************************************************************************************

#include <OrthoTileSource.h>
#include <ossim/init/ossimInit.h>
#include <ossim/base/ossimTimer.h>
#include <ossim/imaging/ossimImageWriterFactoryRegistry.h>
#include <ossim/base/ossimStdOutProgress.h>
#include <iostream>
#include <exception>
#include <getopt.h>
#include <string>

using namespace std;

void usage(const char* argv0, int exitCode, string errMsg="")
{
   if (!errMsg.empty())
      cout<<"\n"<<errMsg<<endl;

   cout<<
       "\nStandalone orthorectification using OSSIM. "
       "\n"
       "\nUsage: "<<argv0<<" [options] <input-image> <output-image>"
       "\n"
       "\nOptions:"
       "\n  -h              Shows this usage."
       "\n  -r <resampler>  Set the type of resampler: \"NN\"=NearestNeighbor, \"BI\"=Bilinear "
       "\n                  Interpolation (default), \"LWM\"=Linear Weighted Mean 3x3."
       "\n"<<endl;

   exit(exitCode);
}


int main (int argc, char **argv)
{
   string appName(argv[0]);
   string resampler_type;

   // Loads OSSIM plugins and preferences:
   auto ossim_init = ossimInit::instance();
   ossim_init->initialize(argc, argv);

   // Parse command line:
   static struct option long_options[] = {{ 0, 0, 0, 0}};
   int c, optIndex=0;
   while ((c = getopt_long(argc, argv, "hr:", long_options, &optIndex)) != -1)
   {
      switch (c)
      {
      case 'h':
         usage(argv[0], 0);
         break;
      case 'r':
         resampler_type = optarg;
         break;
      case '?':
         usage(argv[0], 1);
         break;
      default:
         abort();
      }
   }
   if (optind >= argc-1)
   {
      usage(argv[0], 1, "Error: Filenames required.");
      return 1;
   }

   ossimFilename infile = argv[optind++];
   ossimFilename outFile = argv[optind++];

   try
   {
      // Start the timer.
      ossimTimer timer;
      double timeStart = timer.time_m();

      // This component would start streaming once SDRAM has been filled with pixel data.
      // Presently it works with the output file writer to sequence through ortho-tiles.
      ossimRefPtr<OrthoTileSource> orthoComp = new OrthoTileSource;
      orthoComp->init(infile, resampler_type);
      ossimRefPtr<ossimImageGeometry> geom = orthoComp->getImageGeometry();
      ossimIpt imgSize (geom->getImageSize());

      // Create writer:
      ossimRefPtr<ossimImageFileWriter> writer =
         ossimImageWriterFactoryRegistry::instance()->createWriterFromExtension(outFile.ext());
      if (!writer)
         throw runtime_error( "Unable to create writer given filename extension." );

      writer->connectMyInputTo(orthoComp.get());
      writer->setFilename(outFile);
      writer->setTileSize(ossimIpt(32,32));
      if ((imgSize.x <= 32) && (imgSize.y <= 32))
         writer->setOutputImageType("tiff_strip");
      writer->setPixelType(OSSIM_PIXEL_IS_POINT);

      ossimSetNotifyStream(&std::cout);
      ossimStdOutProgress prog(0, true);
      writer->addListener(&prog);
      writer->initialize();

      if (writer->getErrorStatus() != ossimErrorCodes::OSSIM_OK)
         throw runtime_error( "Unable to initialize writer for execution" );

      if (!writer->execute())
         throw runtime_error("Error encountered writing TIFF.");

      writer->close();
      writer->removeListener(&prog);
      writer = 0;

      // Stop the timer and report:
      uint64_t timeDelta = (uint64_t) round(timer.time_m() - timeStart);
      cout << "\nFinished writing '"<<outFile<<"'. Elapsed time was "
           << timeDelta<<" ms" << endl;

      orthoComp = 0;
   }
   catch (std::exception& e)
   {
      cout<<e.what();
      return 1;
   }

   return 0;
}
