//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//     Author: Oscar Kramer, oscarkramer@yahoo.com.com
//
//**************************************************************************************************

#include "OrthoTileSource.h"
#include <ossim/base/ossimGrect.h>
#include <ossim/imaging/ossimImageHandlerRegistry.h>
#include <ossim/imaging/ossimCacheTileSource.h>
#include <ossim/imaging/ossimImageDataFactory.h>
#include <ossim/projection/ossimEquDistCylProjection.h>
#include <ossim/base/ossimPreferences.h>
#include <ossim/base/ossimString.h>

using namespace std;

const double RADIUS_OF_INFLUENCE_DEFAULT = 0.6;
const uint32_t SAFETY_MARGIN_DEFAULT = 2;
const OrthoTileSource::ResamplerType RESAMPLER_TYPE_DEFAULT = OrthoTileSource::BI;

OrthoTileSource::OrthoTileSource ()
   : m_radius(RADIUS_OF_INFLUENCE_DEFAULT), // radius of influence of input pixel in image space
     m_safetyMargin(SAFETY_MARGIN_DEFAULT),
     m_southPts(0),
     m_centerPts(0),
     m_resamplerType(RESAMPLER_TYPE_DEFAULT),
     m_kernel(new double[9])
{
   // Look up settings from OSSIM config file:
   auto ossimPrefs = ossimPreferences::instance();
   ossimString value = ossimPrefs->findPreference("orthoism.radiusOfInfluence");
   if (!value.empty())
   {
      m_radius = value.toDouble();
      if (m_radius < 0.1)
         m_radius = RADIUS_OF_INFLUENCE_DEFAULT;
   }
   value = ossimPrefs->findPreference("orthoism.safetyMargin");
   if (!value.empty())
   {
      m_safetyMargin = value.toUInt32();
      if (m_safetyMargin < 1)
         m_radius = SAFETY_MARGIN_DEFAULT;
   }
   value = ossimPrefs->findPreference("orthoism.resamplerType");
   setResamplerType(value);
}

OrthoTileSource::~OrthoTileSource ()
{
   disconnectAllInputs();
   delete m_southPts;
   delete m_centerPts;
   delete [] m_kernel;
   m_mapTile = 0;
   m_imageTile = 0;
   m_transform = 0;
   m_inputSource = 0;
}

void OrthoTileSource::setResamplerType(const string& value)
{
   if (value.empty())
      m_resamplerType = RESAMPLER_TYPE_DEFAULT;
   else if (value == "NN")
      m_resamplerType = NN;
   else if (value == "LWM")
      m_resamplerType = LWM;
   else if (value == "DWM")
      m_resamplerType = DWM;
   else if (value == "BI")
      m_resamplerType = BI;
   else
      throw ("ERROR OrthoTileSource::OrthoTileSource() -- Bad resampler type specified in preferences.");
}

void OrthoTileSource::init (const ossimFilename& inputImageFile, const string& resampler_type)
{
   ostringstream xmsg;
   xmsg << "ERROR OrthoTileSource::init(): ";

   setResamplerType(resampler_type);

   // This component simulates the TCP IO and ingress components. It actually opens the input
   // image file and readies the processing chain to accept getTile() calls:
   ossimRefPtr<ossimImageHandler> handler;
   ossimRefPtr<ossimImageSource> cache;
   handler = ossimImageHandlerRegistry::instance()->open(inputImageFile);
   if (!handler)
   {
      xmsg << "Null handler returned opening input file <" << inputImageFile << ">";
      throw (xmsg.str());
   }
   uint32_t numBands = handler->getNumberOfInputBands();
   if (numBands < 1)
   {
      xmsg << "Number of bands less than 1! <" << numBands << ">";
      throw (xmsg.str());
   }

   // Attach a tile cache to avoid multiple disk reads:
   cache = new ossimCacheTileSource;
   cache->connectMyInputTo(handler.get());

   // FPGA: This operation is equivalent to connecting the input stream, except _this_ object will
   // pull tiles from the input connection:
   m_inputSource = cache.get();
   m_numBands = m_inputSource->getNumberOfOutputBands();
   m_nullPix = m_inputSource->getNullPixelValue(0);
   connectMyInputTo(m_inputSource.get());

   // OSSIM Specific. Allocate output tile buffer. Map tiles assumed SQUARE:
   m_mapTile = ossimImageDataFactory::instance()->create(this,this);
   m_mapTile->initialize();
   uint32_t tileSize = m_mapTile->getWidth();
   m_centerPts = new ossimDpt [tileSize+1]; // buffer center and east direction at each column in map space
   m_southPts = new ossimDpt [tileSize]; // buffer south direction at each column in map space

   // Set up output projection given input location and GSD:
   initOutputGeometry();
#if 0
   // TEST round trips:
   ossimDpt imgTP, mapTP, imgTPP;
   imgGeom->getTiePoint(imgTP, false);
   cout<<"\nimgTP: "<<imgTP<<endl;
   m_transform->imageToView(imgTP, mapTP);
   cout<<"mapTP: "<<mapTP<<endl;
   m_transform->viewToImage(mapTP, imgTPP);
   cout<<"imgTPP: "<<imgTPP<<endl;
   cout<<"diff: "<<imgTPP-imgTP<<endl;
   cout<<endl;

   // TEST image bounds:
   m_transform->imageToView(imgRect.ul(), mapTP);
   cout<<"UL img: "<<imgRect.ul()<<"   map: "<<mapTP<<endl;
   m_transform->imageToView(imgRect.ur(), mapTP);
   cout<<"UR img: "<<imgRect.ur()<<"   map: "<<mapTP<<endl;
   m_transform->imageToView(imgRect.lr(), mapTP);
   cout<<"LR img: "<<imgRect.lr()<<"   map: "<<mapTP<<endl;
   m_transform->imageToView(imgRect.ll(), mapTP);
   cout<<"LL img: "<<imgRect.ll()<<"   map: "<<mapTP<<endl;
   cout<<endl;
#endif
}

void OrthoTileSource::initOutputGeometry ()
{
   // This is the same implementation as OrthoIngressClient::initOutputGeometry()

   // Set up the RPC projection:
   ossimRefPtr<ossimImageGeometry> imgGeom = m_inputSource->getImageGeometry();
   if (!imgGeom)
      throw("ERROR OrthoTileSource::init() -- Null geometry encountered!");

   ossimDrect inputImageRect;
   imgGeom->getBoundingRect(inputImageRect);
   ossimGpt gUL, gUR, gLR, gLL;
   imgGeom->localToWorld(inputImageRect.ul(), gUL);
   imgGeom->localToWorld(inputImageRect.ur(), gUR);
   imgGeom->localToWorld(inputImageRect.lr(), gLR);
   imgGeom->localToWorld(inputImageRect.ll(), gLL);

   ossimRefPtr<ossimMapProjection> proj = new ossimEquDistCylProjection;

   ossimGrect gndRect(gUL, gUR, gLR, gLL);
   proj->setOrigin(gndRect.midPoint());
   ossimDpt gsd (imgGeom->getMetersPerPixel());
   gsd.y = gsd.x = gsd.mean();
   proj->setMetersPerPixel(gsd);
   proj->setUlTiePoints(gUL);

   // Determine ideal map rotation. First determine azimuth of +Y axis then apply that as rotation:
   double mapRotation = gLL.azimuthTo(gUL);
   if ((mapRotation < 359.5) && (mapRotation > 0.5))
      proj->applyRotation(mapRotation);

   // Now project image corner ground coordinates through the output projection to get output
   // corners and corresponding image size:
   ossimDpt outUL (proj->worldToLineSample(gUL));
   ossimDpt outUR (proj->worldToLineSample(gUR));
   ossimDpt outLR (proj->worldToLineSample(gLR));
   ossimDpt outLL (proj->worldToLineSample(gLL));
   ossimIrect outputImageRect (outUL, outUR, outLR, outLL);

   // The rectangle may have expanded so that UL is not 0,0. Reproject a new UL point to force that:
   proj->lineSampleToWorld(outputImageRect.ul(), gUL);
   proj->setUlTiePoints(gUL);
   outUL = proj->worldToLineSample(gUL);// - shift; // should be at (0,0)
   outUR = proj->worldToLineSample(gUR);// - shift;
   outLR = proj->worldToLineSample(gLR);// - shift;
   outLL = proj->worldToLineSample(gLL);// - shift;
   outputImageRect = ossimIrect (outUL, outUR, outLR, outLL);

   // Finally, initialize the output geometry object:
   m_mapGeom = new ossimImageGeometry();
   m_mapGeom->setProjection(proj.get());
   m_mapGeom->setImageSize(outputImageRect.size());

   m_transform = new ossimImageViewProjectionTransform(imgGeom.get(), m_mapGeom.get());

   //clog<<"orthoism Output Projection: "<<endl;
   //proj->print(clog);
}


ossimRefPtr<ossimImageData> OrthoTileSource::getTile (const ossimIrect &mapRect, ossim_uint32 resLevel)
{
   // Initialize output tile buffer to be streamed out. These ops are OSSIM-specific:
   m_mapTile->makeBlank();
   m_mapTile->setImageRectangle(mapRect);

   const ossimIpt ulMap (mapRect.ul());
   const ossimIpt urMap (mapRect.ur());
   const ossimIpt lrMap (mapRect.lr());
   const ossimIpt llMap (mapRect.ll());

   // Transform the corners of map tile from map-space to input image space to get footprint:
   const ossimIpt cornerUl (mapToImage(ulMap));
   const ossimIpt cornerUr (mapToImage(urMap));
   const ossimIpt cornerLr (mapToImage(lrMap));
   const ossimIpt cornerLl (mapToImage(llMap));

   // FPGA: Load BRAM with all contributing input tiles:
   loadImageTile(cornerUl, cornerUr, cornerLr, cornerLl);

   // OSSIM-specific, check tile statusd and skip resample if emplty:
   if (m_imageTile->getDataObjectStatus() == OSSIM_EMPTY)
      return m_mapTile;

   //cout<<"Processing map tile "<<ulMap<<endl;

   // Buffer the projected points for the first row of map pixels. Note additional point to east:
   ossimIpt mapPt(ulMap);
   uint32_t i=0;
   for (; mapPt.u<=lrMap.u+1; ++mapPt.u)
   {
      m_centerPts[i++] = mapToImage(mapPt);
   }

   // Loop over each map tile pixel and resample given input pixels:
   for (mapPt.v=ulMap.v; mapPt.v<=lrMap.v; ++mapPt.v)
   {
      i=0;
      for (mapPt.u=ulMap.u; mapPt.u<=lrMap.u; ++mapPt.u)
      {
         m_southPts[i] = mapToImage(ossimIpt(mapPt.u, mapPt.v+1));
         switch (m_resamplerType)
         {
         case NN:
            resampleNN(mapPt, m_centerPts[i]);
            break;
         case BI:
            resampleBI(mapPt, m_centerPts[i]);
            break;
         case LWM:
         case DWM:
            resample(mapPt, m_centerPts[i], m_southPts[i], m_centerPts[i+1]);
            break;
         default:
            throw ("ResamplerType not supported");
         }

         // The center pt at [i] will no longer be accessed, write the south pt into it to prepare
         // for next row's pass:
         m_centerPts[i] = m_southPts[i];
         ++i;
      }
      // Still have the east point for next row to compute (u and i have been incremented):
      m_centerPts[i] = mapToImage(ossimIpt(mapPt.u, mapPt.v));
   }

   m_mapTile->validate();
   return m_mapTile;
}

ossimDpt OrthoTileSource::mapToImage (const ossimIpt& mapPt)
{
   // Using OSSIM for the moment:
   ossimDpt oMapPt (mapPt);
   ossimDpt oImgPt;
   m_transform->viewToImage(oMapPt, oImgPt);
   return oImgPt;
}


void OrthoTileSource::resample(const ossimIpt& mapPt, const ossimDpt& P, const ossimDpt& E, const ossimDpt& S)
{
   // TODO: Presently implemented for 3x3 kernel only.
   // Establish integral (P', nearest neighbor) pixel location in image space:
   ossimIpt Pnn (P);

   // Compute map pixel extents in image space Rx, Ry:
   double x1 = fabs(P.x - E.x + P.y - E.y);
   double y1 = fabs(P.x - S.x + P.y - S.y);
   double x2 = fabs(E.x - P.x + P.y - E.y);
   double y2 = fabs(S.x - P.x + P.y - S.y);
   double Rx = 0.5 * max(x1, x2);
   double Ry = 0.5 * max(y1, y2);

   // Determine weights on x and y directions:
   double dx1 = m_radius + Rx - fabs(Pnn.x-1 - P.x);
   double dx2 = m_radius + Rx - fabs(Pnn.x   - P.x);
   double dx3 = m_radius + Rx - fabs(Pnn.x+1 - P.x);
   double dy1 = m_radius + Ry - fabs(Pnn.y-1 - P.y);
   double dy2 = m_radius + Ry - fabs(Pnn.y   - P.y);
   double dy3 = m_radius + Ry - fabs(Pnn.y+1 - P.y);

   // Clamp the negative 1D weights to 0:
   if (dx1 < 0) dx1 = 0;
   if (dx2 < 0) dx2 = 0;
   if (dx3 < 0) dx3 = 0;
   if (dy1 < 0) dy1 = 0;
   if (dy2 < 0) dy2 = 0;
   if (dy3 < 0) dy3 = 0;

   if (m_resamplerType == LWM)
      computeLWM(dx1, dx2, dx3, dy1, dy2, dy3);
   else /*if (m_resamplerType == DWM) */
      computeDWM(dx1, dx2, dx3, dy1, dy2, dy3);

   // Loop for each band:
   double B, W;
   double p11, p12, p13, p21, p22, p23, p31, p32, p33;
   for (uint32_t band = 0; band < m_numBands; ++band)
   {
      // Fetch the 3x3 input kernel pixels. Unfortunately, every input pixel needs to be checked
      // for null value, and exclude it from the kernel by setting its weight to 0:
      p11 = m_imageTile->getPix(ossimIpt(Pnn.x-1, Pnn.y-1), band);
      if (p11 == m_nullPix)
         m_kernel[0] = 0;
      p12 = m_imageTile->getPix(ossimIpt(Pnn.x-1, Pnn.y  ), band);
      if (p12 == m_nullPix)
         m_kernel[1] = 0;
      p13 = m_imageTile->getPix(ossimIpt(Pnn.x-1, Pnn.y+1), band);
      if (p13 == m_nullPix)
         m_kernel[2] = 0;
      p21 = m_imageTile->getPix(ossimIpt(Pnn.x  , Pnn.y-1), band);
      if (p21 == m_nullPix)
         m_kernel[3] = 0;
      p22 = m_imageTile->getPix(Pnn, band); // nearest neighbor
      if (p22 == m_nullPix)
         m_kernel[4] = 0;
      p23 = m_imageTile->getPix(ossimIpt(Pnn.x  , Pnn.y+1), band);
      if (p23 == m_nullPix)
         m_kernel[5] = 0;
      p31 = m_imageTile->getPix(ossimIpt(Pnn.x+1, Pnn.y-1), band);
      if (p31 == m_nullPix)
         m_kernel[6] = 0;
      p32 = m_imageTile->getPix(ossimIpt(Pnn.x+1, Pnn.y  ), band);
      if (p32 == m_nullPix)
         m_kernel[7] = 0;
      p33 = m_imageTile->getPix(ossimIpt(Pnn.x+1, Pnn.y+1), band);
      if (p33 == m_nullPix)
         m_kernel[8] = 0;

      // Apply kernel to input and compute weighted mean:
      W =   m_kernel[0] + m_kernel[1] + m_kernel[2] +
            m_kernel[3] + m_kernel[4] + m_kernel[5] +
            m_kernel[6] + m_kernel[7] + m_kernel[8];
      if (W > 0.0)
      {
         B = (m_kernel[0] * p11 + m_kernel[1] * p12 + m_kernel[2] * p13 +
              m_kernel[3] * p21 + m_kernel[4] * p22 + m_kernel[5] * p23 +
              m_kernel[6] * p31 + m_kernel[7] * p32 + m_kernel[8] * p33) / W;
      }
      else
      {
         B = m_nullPix;
      }
      m_mapTile->setValue(mapPt.u, mapPt.v, B, band);
   }
}

void OrthoTileSource::computeLWM(const double& dx1, const double& dx2, const double& dx3,
                                 const double& dy1, const double& dy2, const double& dy3)
{
   // Pre-compute kernel weights:
   m_kernel[0] = dx1*dy1;
   m_kernel[1] = dx1*dy2;
   m_kernel[2] = dx1*dy3;
   m_kernel[3] = dx2*dy1;
   m_kernel[4] = dx2*dy2;
   m_kernel[5] = dx2*dy3;
   m_kernel[6] = dx3*dy1;
   m_kernel[7] = dx3*dy2;
   m_kernel[8] = dx3*dy3;
}

void OrthoTileSource::computeDWM(const double& dx1, const double& dx2, const double& dx3,
                                 const double& dy1, const double& dy2, const double& dy3)
{
   // Pre-compute kernel weights:
   m_kernel[0] = sqrt(dx1*dx1 + dy1*dy1);
   m_kernel[1] = sqrt(dx1*dx1 + dy2*dy2);
   m_kernel[2] = sqrt(dx1*dx1 + dy3*dy3);
   m_kernel[3] = sqrt(dx2*dx2 + dy1*dy1);
   m_kernel[4] = sqrt(dx2*dx2 + dy2*dy2);
   m_kernel[5] = sqrt(dx2*dx2 + dy3*dy3);
   m_kernel[6] = sqrt(dx3*dx3 + dy1*dy1);
   m_kernel[7] = sqrt(dx3*dx3 + dy2*dy2);
   m_kernel[8] = sqrt(dx3*dx3 + dy3*dy3);
}
void OrthoTileSource::resampleNN(const ossimIpt& mapPt, const ossimDpt& P)
{
   // Establish integral (P') pixel location in image space:
   ossimIpt Pnn (P);

   // Loop for each band:
   double B;
   for (uint32_t band = 0; band < m_numBands; ++band)
   {
      B = m_imageTile->getPix(Pnn, band);
      m_mapTile->setValue(mapPt.u, mapPt.v, B, band);
   }
}

void OrthoTileSource::resampleBI(const ossimIpt& mapPt, const ossimDpt& P)
{
   ossimIpt Pul ((int)P.x, (int)P.y);
   ossimIpt Pur (Pul.x+1, Pul.y);
   ossimIpt Plr (Pul.x+1, Pul.y+1);
   ossimIpt Pll (Pul.x,   Pul.y+1);

   double dx = P.x - Pul.x;
   double dy = P.y - Pul.y;
   double dxc = 1.0 - dx;
   double dyc = 1.0 - dy;
   double B;
   double p11, p12, p21, p22;

   for (uint32_t band = 0; band < m_numBands; ++band)
   {
      p11 = m_imageTile->getPix(Pul, band);
      if (p11 == m_nullPix)
         p11 = 0;
      p12 = m_imageTile->getPix(Pur, band);
      if (p12 == m_nullPix)
         p12 = 0;
      p21 = m_imageTile->getPix(Pll, band);
      if (p21 == m_nullPix)
         p21 = 0;
      p22 = m_imageTile->getPix(Plr, band);
      if (p22 == m_nullPix)
         p22 = 0;
      B = dxc*dyc*p11 + dx*dyc*p12 + dx*dy*p22 + dxc*dy*p21;
      if (B < 0)
         B = 0;
      m_mapTile->setValue(mapPt.u, mapPt.v, B, band);
   }
}

void OrthoTileSource::loadImageTile (const ossimIpt& corner1,
                                     const ossimIpt& corner2,
                                     const ossimIpt& corner3,
                                     const ossimIpt& corner4)
{
   // Establish bounding rect in image space for map tile's footprint, and adjust for safety margin:
   ossimIpt imgUl (INT32_MAX, INT32_MAX);
   ossimIpt imgLr (INT32_MIN, INT32_MIN);

   if (corner1.x < imgUl.x)  imgUl.x = corner1.x - m_safetyMargin;
   if (corner2.x < imgUl.x)  imgUl.x = corner2.x - m_safetyMargin;
   if (corner3.x < imgUl.x)  imgUl.x = corner3.x - m_safetyMargin;
   if (corner4.x < imgUl.x)  imgUl.x = corner4.x - m_safetyMargin;

   if (corner1.y < imgUl.y)  imgUl.y = corner1.y - m_safetyMargin;
   if (corner2.y < imgUl.y)  imgUl.y = corner2.y - m_safetyMargin;
   if (corner3.y < imgUl.y)  imgUl.y = corner3.y - m_safetyMargin;
   if (corner4.y < imgUl.y)  imgUl.y = corner4.y - m_safetyMargin;

   if (corner1.x > imgLr.x)  imgLr.x = corner1.x + m_safetyMargin;
   if (corner2.x > imgLr.x)  imgLr.x = corner2.x + m_safetyMargin;
   if (corner3.x > imgLr.x)  imgLr.x = corner3.x + m_safetyMargin;
   if (corner4.x > imgLr.x)  imgLr.x = corner4.x + m_safetyMargin;

   if (corner1.y > imgLr.y)  imgLr.y = corner1.y + m_safetyMargin;
   if (corner2.y > imgLr.y)  imgLr.y = corner2.y + m_safetyMargin;
   if (corner3.y > imgLr.y)  imgLr.y = corner3.y + m_safetyMargin;
   if (corner4.y > imgLr.y)  imgLr.y = corner4.y + m_safetyMargin;

   // Make sure UL didn't underflow: TODO: Can negative image coordinates exist?
   if (imgUl.x < 0) imgUl.x = 0;
   if (imgUl.y < 0) imgUl.y = 0;

   // FPGA: This function will copy the contributing input tiles to BRAM.
   // TODO: Need to decide how pixels are organized in BRAM. Here they stay tiled to take advantage
   //       of OSSIM tile caching
   ossimIrect imgTileRect (imgUl, imgLr);
   m_imageTile = m_inputSource->getTile(imgTileRect);
}


void OrthoTileSource::error (const char *msg) const
{
   perror(msg);
}

bool OrthoTileSource::canConnectMyInputTo(ossim_int32 /*X*/, const ossimConnectableObject *obj) const
{
   return (bool) dynamic_cast<const ossimImageSource *>(obj);
}

ossim_uint32 OrthoTileSource::getNumberOfInputBands () const
{
   return m_numBands;
}

ossim_uint32 OrthoTileSource::getNumberOfOutputBands () const
{
   return m_numBands;
}

