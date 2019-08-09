//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//     Author: Oscar Kramer, oscarkramer@yahoo.com
//
//**************************************************************************************************

#ifndef OrthoTileSource_H
#define OrthoTileSource_H

#include <ossim/base/ossimRefPtr.h>
#include <ossim/imaging/ossimImageSource.h>
#include <ossim/projection/ossimImageViewProjectionTransform.h>

class OrthoTileSource : public ossimImageSource
{
public:
   enum ResamplerType { NN  /* Nearest Neighbor */,
                        LWM /* Linear Weighted Mean */,
                        DWM /* Distance-weighted Mean */,
                        BI  /* Bilinear Interpolation */ };

   OrthoTileSource ();

   ~OrthoTileSource () override;

   /**
    * Accepts input pixel source (instead of stream).
    * @param inputSource upstream component
    * @throws exception
    */
   void init(const ossimFilename& inputImageFile, const std::string& resampler_type);

   void exit() {}

   ossimRefPtr<ossimImageData> getTile (const ossimIrect &rect, ossim_uint32 resLevel) override;

   // The remaining methods serve to fulfull the pure virtuals in ossimImageSource
   bool canConnectMyInputTo (ossim_int32, const ossimConnectableObject *obj) const override;

   void initialize () override {}

   /**
    * Fulfills ossimImageSource requirement
    */
   ossim_uint32 getNumberOfInputBands () const override;
   ossim_uint32 getNumberOfOutputBands () const override;

   /**
    * Fulfills ossimImageSource requirement. Returns map's rect.
    */
   void  getAreaOfInterest(ossimIrect& mapRect) const { mapRect = m_mapRect; }

   /**
    * Fulfills ossimImageSource requirement, returns map projection geometry
    */
   virtual ossimRefPtr<ossimImageGeometry> getImageGeometry() override { return m_mapGeom; }

private:
   void error (const char *msg) const;

   /** Computes the output map geometry **/
   void initOutputGeometry ();

   /** Performs transform from map space to image space via map projection and RPC functions */
   ossimDpt mapToImage (const ossimIpt& mapPt);

   void loadImageTile(const ossimIpt& corner1,
                      const ossimIpt& corner2,
                      const ossimIpt& corner3,
                      const ossimIpt& corner4);

   void setResamplerType(const std::string& rtype);

   /** Resamples input to produce output at u, v. Result is inserted into output map tile. */
   void resample(const ossimIpt& mapPt,
                 const ossimDpt& centerImgPt,
                 const ossimDpt& eastImgPt,
                 const ossimDpt& southImgPt);

   /** Computes kernel weights for the Distance Weighted Mean kernel. */
   void computeDWM(const double& dx1, const double& dx2, const double& dx3,
                   const double& dy1, const double& dy2, const double& dy3);

   /** Computes kernel weights for the Linear Weighted Mean kernel. */
   void computeLWM(const double& dx1, const double& dx2, const double& dx3,
                   const double& dy1, const double& dy2, const double& dy3);

   /** Resamples input to produce output at u, v. This resampler utilizes the Nearest neighbor
    * method. Result is inserted into output map tile. */
   void resampleNN(const ossimIpt& mapPt, const ossimDpt& centerImgPt);

   /** Resamples input to produce output at u, v. This resampler utilizes the bilinear interpolation
    * method. Result is inserted into output map tile. */
   void resampleBI(const ossimIpt& mapPt, const ossimDpt& centerImgPt);

   ossimRefPtr<ossimImageSource> m_inputSource; // Provides pixels
   ossimRefPtr<ossimImageData> m_mapTile; // output tile buffer
   ossimRefPtr<ossimImageData> m_imageTile; // input tile buffer
   ossimRefPtr<ossimImageGeometry> m_mapGeom; // Contains projection for use by egress
   double m_radius;
   uint32_t m_safetyMargin;
   uint32_t m_numBands;
   ossimRefPtr<ossimImageViewProjectionTransform> m_transform;
   ossimDpt* m_southPts; // Buffering to avoid duplicate mapToImage() call
   ossimDpt* m_centerPts; // Buffering to avoid duplicate mapToImage() call
   ossimIrect m_mapRect;
   ResamplerType m_resamplerType;
   double m_nullPix;
   double* m_kernel;
};
#endif
