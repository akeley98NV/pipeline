// Copyright (c) 2002-2016, NVIDIA CORPORATION. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// DPBFLoader.cpp : Defines the entry point for the DLL application.
//

#include <dp/Exception.h>
#include <dp/fx/EffectLibrary.h>
#include <dp/sg/core/Billboard.h>
#include <dp/sg/core/ClipPlane.h>
#include <dp/sg/core/FrustumCamera.h>
#include <dp/sg/core/GeoNode.h>
#include <dp/sg/core/LightSource.h>
#include <dp/sg/core/LOD.h>
#include <dp/sg/core/MatrixCamera.h>
#include <dp/sg/core/ParallelCamera.h>
#include <dp/sg/core/PerspectiveCamera.h>
#include <dp/sg/core/Primitive.h>
#include <dp/sg/core/Sampler.h>
#include <dp/sg/core/Scene.h>
#include <dp/sg/core/Switch.h>
#include <dp/sg/core/TextureHost.h>
#include <dp/sg/core/Transform.h>
#include <dp/sg/core/VertexAttribute.h>
#include <dp/sg/ui/ViewState.h>
#include <dp/sg/io/PlugInterfaceID.h>
#include <dp/sg/io/IO.h>
#include <dp/util/File.h>
#include <dp/util/Locale.h>
#include <dp/sg/io/DPBF/Loader/inc/DPBFLoader.h>
#include <set>
#include <sstream>

using namespace dp::sg::core;
using namespace dp::math;
using namespace dp::util;
using std::make_pair;
using std::map;
using std::vector;
using std::string;
using std::stringstream;
using std::pair;
using std::set;
using std::find;

// supported Plug Interface ID
const UPITID PITID_SCENE_LOADER(UPITID_SCENE_LOADER, UPITID_VERSION); // plug-in type
const UPIID  PIID_NBF_SCENE_LOADER(".NBF", PITID_SCENE_LOADER); // plug-in ID
const UPIID  PIID_DPBF_SCENE_LOADER(".DPBF", PITID_SCENE_LOADER); // plug-in ID

// convenient macro
#define INVOKE_CALLBACK(cb) if ( callback() ) callback()->cb

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// module-private nbf structure defines to provide downward compatibility

// NBF is independent of packing. However, this forces
// the compile-time asserts below to fire on inconsistencies
#pragma pack(push, 1)

struct NBFPatchesBase_nbf_47 : public NBFIndependentPrimitiveSet
{
  uint_t    verticesPerPatch;
  PADDING(4);                    //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFPatchesBase_nbf_47) % 8 ) == 0 );

struct NBFPatches_nbf_47 : public NBFPatchesBase_nbf_47
{
};
DP_STATIC_ASSERT( ( sizeof(NBFPatches_nbf_47) % 8 ) == 0 );

struct NBFQuadPatches_nbf_47 : public NBFPatchesBase_nbf_47
{
  uint_t  size;
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches_nbf_47) % 8 ) == 0 );

struct NBFQuadPatches4x4_nbf_47 : public NBFPatchesBase_nbf_47
{
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches4x4_nbf_47) % 8 ) == 0 );

struct NBFRectPatches_nbf_47 : public NBFPatchesBase_nbf_47
{
  uint_t  width;
  uint_t  height;
};
DP_STATIC_ASSERT( ( sizeof(NBFRectPatches_nbf_47) % 8 ) == 0 );

struct NBFTriPatches_nbf_47 : public NBFPatchesBase_nbf_47
{
  uint_t  size;
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches_nbf_47) % 8 ) == 0 );

struct NBFTriPatches4_nbf_47 : public NBFPatchesBase_nbf_47
{
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches4_nbf_47) % 8 ) == 0 );


struct NBFMaterial_nbf_a : public NBFObject
{
  float3_t    ambientColor;      //!< Specifies the ambient part of the front material color.
  float3_t    diffuseColor;      //!< Specifies the diffuse part of the front material color.
  float3_t    emissiveColor;     //!< Specifies the emissive part of the front material color.
  float       opacity;           //!< Specifies the opacity of the front material.
  float3_t    specularColor;     //!< Specifies the specular part of the front material color.
  float       specularExponent;  //!< Specifies the specular exponent of the front material color.
};
DP_STATIC_ASSERT( ( sizeof(NBFMaterial_nbf_a) % 8 ) == 0 );

struct NBFScene_nbf_b
{
  float3_t    backColor;                //!< Specifies the scene's RGB background color used with rendering.
  uint_t      numCameras;               //!< Specifies the number of the scene's NBFCamera objects.
  uint_t      cameras;                  //!< Specifies the file offset to the offsets of the scene's NBFCamera objects.
  uint_t      numCameraAnimations;      //!< Specifies the number of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      cameraAnimations;         //!< Specifies the offset to the offsets of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      numberOfAnimationFrames;  //!< For animated scenes, this specifies the number of animation frames.
  uint_t      root;                     //!< Specifies the file offset to the scene's root node, which always is of a NBFNode derived type.
};
DP_STATIC_ASSERT( ( sizeof(NBFScene_nbf_b) % 4 ) == 0 );

struct NBFScene_nbf_31
{
  float3_t    ambientColor;             //!< Specifies the global ambient color to be used for rendering.
  float3_t    backColor;                //!< Specifies the scene's RGB background color used with rendering.
  uint_t      numCameras;               //!< Specifies the number of the scene's NBFCamera objects.
  uint_t      cameras;                  //!< Specifies the file offset to the offsets of the scene's NBFCamera objects.
  uint_t      numCameraAnimations;      //!< Specifies the number of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      cameraAnimations;         //!< Specifies the offset to the offsets of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      numberOfAnimationFrames;  //!< For animated scenes, this specifies the number of animation frames.
  uint_t      root;                     //!< Specifies the file offset to the scene's root node, which always is of a NBFNode derived type.
};
DP_STATIC_ASSERT( ( sizeof(NBFScene_nbf_31) % 4 ) == 0 );

//! The NBFScene structure represents a scene in the context of computer graphics.
/** A valid NBF file always contains one - and only one - NBFScene object.
* The file offset to this NBFScene object is specified within the NBFHeader structure. */
struct NBFScene_nbf_37
{
  float3_t    ambientColor;             //!< Specifies the global ambient color to be used for rendering.
  float4_t    backColor;                //!< Specifies the scene's RGBA background color used with rendering.
  uint_t      numCameras;               //!< Specifies the number of the scene's NBFCamera objects.
  uint_t      cameras;                  //!< Specifies the file offset to the offsets of the scene's NBFCamera objects.
  uint_t      numCameraAnimations;      //!< Specifies the number of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      cameraAnimations;         //!< Specifies the offset to the offsets of the scene's NBFTrafoAnimation_nbf_3d objects (camera animations).
  uint_t      numberOfAnimationFrames;  //!< For animated scenes, this specifies the number of animation frames.
  uint_t      root;                     //!< Specifies the file offset to the scene's root node, which always is of a NBFNode derived type.
};
DP_STATIC_ASSERT( ( sizeof(NBFScene_nbf_37) % 4 ) == 0 );

//! The NBFScene_nbf_3e structure represents a scene in the context of computer graphics.
/** A valid NBF file always contains one - and only one - NBFScene_nbf_3e object.
* The file offset to this NBFScene_nbf_3e object is specified within the NBFHeader structure. */
struct NBFScene_nbf_3e
{
  float3_t    ambientColor;             //!< Specifies the global ambient color to be used for rendering.
  float4_t    backColor;                //!< Specifies the scene's RGBA background color used with rendering.
  uint_t      backImg;                  //!< Specifies the file offset to the back image object
  uint_t      numCameras;               //!< Specifies the number of the scene's NBFCamera objects.
  uint_t      cameras;                  //!< Specifies the file offset to the offsets of the scene's NBFCamera objects.
  uint_t      numCameraAnimations;      //!< Specifies the number of the scene's NBFTrafoAnimation objects (camera animations).
  uint_t      cameraAnimations;         //!< Specifies the offset to the offsets of the scene's NBFTrafoAnimation objects (camera animations).
  uint_t      numberOfAnimationFrames;  //!< For animated scenes, this specifies the number of animation frames.
  uint_t      root;                     //!< Specifies the file offset to the scene's root node, which always is of a NBFNode derived type.
};
DP_STATIC_ASSERT( ( sizeof(NBFScene_nbf_3e) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFScene_nbf_41 structure represents a scene in the context of computer graphics.
/** A valid NBF file always contains one - and only one - NBFScene_nbf_41 object.
* The file offset to this NBFScene_nbf_41 object is specified within the NBFHeader structure. */
struct NBFScene_nbf_41
{
  float3_t    ambientColor;             //!< Specifies the global ambient color to be used for rendering.
  float4_t    backColor;                //!< Specifies the scene's RGBA background color used with rendering.
  uint_t      backImg;                  //!< Specifies the file offset to the back image object
  uint_t      numCameras;               //!< Specifies the number of the scene's NBFCamera objects.
  uint_t      cameras;                  //!< Specifies the file offset to the offsets of the scene's NBFCamera objects.
  uint_t      numCameraAnimations;      //!< Specifies the number of the scene's NBFTrafoAnimation objects (camera animations).
  uint_t      cameraAnimations;         //!< Specifies the offset to the offsets of the scene's NBFTrafoAnimation objects (camera animations).
  uint_t      numberOfAnimationFrames;  //!< For animated scenes, this specifies the number of animation frames.
  uint_t      root;                     //!< Specifies the file offset to the scene's root node, which always is of a NBFNode derived type.
  uint_t      numObjectLinks;           //!< Specifies the number of objects links in the scene
  uint_t      objectLinks;              //!< Specifies the file offset to the scenes's object links
};
DP_STATIC_ASSERT( ( sizeof(NBFScene_nbf_41) % 4 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFFaceAttribute_nbf_b : public NBFObject
{
  float3_t    ambientColor;             //!< Specifies the global ambient color to be used for rendering.
  ubyte_t     cullMode;             //!< Specifies the face culling mode.
  ubyte_t     polygonOffsetEnabled; //!< Specifies if polygon offset should be enabled for rendering.
  PADDING(2);                       //!< Padding bits to ensure offset of polygonOffsetFactor is on a 4-byte boundary, regardless of packing
  float       polygonOffsetFactor;  //!< Specifies the scale factor to be used to create a variable depth offset
  //!< for a polygon.
  float       polygonOffsetUnits;   //!< Specifies a unit that is multiplied by a render API implementation-specific
  //!< value to create a constant depth offset.
  ubyte_t     twoSidedLighting;     //!< Specifies if two-sided lighting should be enabled for rendering.
  PADDING(7);                       //!< Padding bits to ensure the size of NBFFaceAttribute_nbf_8 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFFaceAttribute_nbf_b) % 8 ) == 0 );

struct geometrySet_t_nbf_d
{
  uint_t      primitive;        //!< Specifies the file offset to the NBFPrimitive object.
  uint_t      stateSet;         //!< Specifies the file offset to the corresponding NBFStateSet object.
};
DP_STATIC_ASSERT( ( sizeof(geometrySet_t_nbf_d) % 4 ) == 0 );

struct trafo_t_nbf_f
{
  float4_t    orientation;        //!< Specifies the orientational part of the transformation.
  float3_t    scaling;            //!< Specifies the scaling part of the transformation.
  float3_t    translation;        //!< Specifies the translational part of the transformation.
  float3_t    center;             //!< Specifies the center of rotation of the transformation.
};
DP_STATIC_ASSERT( ( sizeof(trafo_t_nbf_f) % 4 ) == 0 );   //!< test size of struct

struct NBFTextureAttributeItem_nbf_e : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  uint_t        texType;            //!< Specifies the texture type. Valid types are TT_AMBIENT, TT_BUMP, TT_DIFFUSE,
                                    //!< TT_DISPLACEMENT, TT_FILTER, TT_OPACITY, TT_REFLECTION, TT_REFRACTION,
                                    //!< TT_SELF_ILLUM, TT_SHININESS, TT_SHINING_STRENGTH, and TT_SPECULAR.
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
                                    //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t_nbf_f trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
  PADDING(4); //!< Padding bits to ensure the size of NBFTextureAttributeItem is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_e) % 8 ) == 0 );

struct NBFTextureAttributeItem_nbf_f : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
  //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t_nbf_f trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_f) % 8 ) == 0 );   //!< test size of struct

struct NBFTextureAttributeItem_nbf_12 : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
  //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t       trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_12) % 8 ) == 0 );   //!< test size of struct

struct NBFTextureAttributeItem_nbf_20 : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  float4_t      texEnvColor;        //!< Specifies the texture environment color.
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
                                    //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t       trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_20) % 8 ) == 0 );

struct NBFTextureAttributeItem_nbf_36 : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  float4_t      texEnvColor;        //!< Specifies the texture environment color.
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
  //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texEnvScale;        //!< Specifies the texture environment scale used with rasterization
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t       trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
  PADDING(4);   //!< Padding bits to ensure the size of NBFTextureAttributeItem is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_36) % 8 ) == 0 );

struct NBFTextureAttributeItem_nbf_4b : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  uint_t        texTarget;          //!< Specifies the texture target
  float4_t      texEnvColor;        //!< Specifies the texture environment color.
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
  //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texEnvScale;        //!< Specifies the texture environment scale used with rasterization
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t       trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_4b) % 8 ) == 0 );

struct NBFGroup_nbf_11 : public NBFNode
{
  uint_t      numChildren;        //!< Specifies the number of maintained children.
  uint_t      children;           //!< Specifies the file offset to the offsets to the maintained children.
  //!< NBFGroup's children always are of NBFNode-derived types.
};
DP_STATIC_ASSERT( ( sizeof(NBFGroup_nbf_11) % 8 ) == 0 );

struct NBFTransform_nbf_f : public NBFGroup_nbf_11
{
  trafo_t_nbf_f trafo;  //!< Specifies the transformation.
  PADDING(4);        //!< Padding bits to ensure the size of NBFTransform is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTransform_nbf_f) % 8 ) == 0 );   //!< test size of struct

struct NBFAnimatedTransform_nbf_f : public NBFTransform_nbf_f
{
  uint_t      animation;          //!< Specifies the file offset to the NBFTrafoAnimation_nbf_3d object
  //!< to be applied to the transform group node.
  PADDING(4);        //!< Padding bits to ensure the size of NBFAnimatedTransform_nbf_f is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedTransform_nbf_f) % 8 ) == 0 );   //!< test size of struct

//! The keyVariant_t structure specifies how a pair of a VariantKey and a StateVariant is stored in a .DPBF file.
struct keyVariant_t
{
  uint_t      key;                //!< Specifies the key of this pair
  uint_t      variant;            //!< Specifies the offset to an NBFStateVariant in the .DPBF file.
};
DP_STATIC_ASSERT( ( sizeof(keyVariant_t) % 4 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFStateSet_nbf_10 : public NBFObject
{
  uint_t numStateVariants;        //!< Specifies the number of contained pairs of VariantKey and StateVariant.
  uint_t keyStateVariantPairs;    //!< Specifies the file offset to the offsets to the keyVariant_t objects
};
DP_STATIC_ASSERT( ( sizeof(NBFStateSet_nbf_10) % 8 ) == 0 );   //!< test size of struct

struct NBFBillboard_nbf_11: public NBFGroup_nbf_11
{
  float3_t  rotationAxis;
  ubyte_t   viewerAligned;
  PADDING(3);        //!< Padding bits to ensure the size of NBFBillboard is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFBillboard_nbf_11) % 8 ) == 0 );

struct NBFLOD_nbf_11 : public NBFGroup_nbf_11
{
  float3_t    center;             //!< Specifies the center point used for distance calculations.
  uint_t      numRanges;          //!< Specifies the number of contained ranges.
  uint_t      ranges;             //!< Specifies the file offset to the ranges.
  //!< Ranges are stored as 32-bit floating point numbers.
  PADDING(4);        //!< Padding bits to ensure the size of NBFLOD is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFLOD_nbf_11) % 8 ) == 0 );

struct NBFSwitch_nbf_11 : public NBFGroup_nbf_11
{
  uint_t      numActiveChildren;  //!< Specifies the number of children that are currently switched on.
  uint_t      activeChildren;     //!< Specifies the offset to indices to the active children.
  //!< Indices are stored as 32-bit unsigned integers.
};
DP_STATIC_ASSERT( ( sizeof(NBFSwitch_nbf_11) % 8 ) == 0 );

struct NBFTransform_nbf_11 : public NBFGroup_nbf_11
{
  trafo_t             trafo;
  PADDING(4);        //!< Padding bits to ensure the size of NBFTransform is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTransform_nbf_11) % 8 ) == 0 );

struct NBFAnimatedTransform_nbf_11 : public NBFTransform_nbf_11
{
  uint_t      animation;          //!< Specifies the file offset to the NBFTrafoAnimation_nbf_3d object
  //!< to be applied to the transform group node.
  PADDING(4);        //!< Padding bits to ensure the size of NBFAnimatedTransform_nbf_11 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedTransform_nbf_11) % 8 ) == 0 );

struct NBFLightSource_nbf_12 : public NBFNode
{
  float       intensity;          //!< Specifies the light intensity.
  float3_t    ambientColor;       //!< Specifies the ambient color term of the light.
  float3_t    diffuseColor;       //!< Specifies the diffuse color term of the light.
  float3_t    specularColor;      //!< Specifies the specular color term of the light.
  ubyte_t     castShadow;         //!< flag that determines if this light source creates shadows.
  PADDING(3);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
  uint_t      animation;          //!< Specifies the file offset to an optional NBFTrafoAnimation_nbf_3d object
  //!< to be applied to the light transform (orientation and translation).
};
DP_STATIC_ASSERT( ( sizeof(NBFLightSource_nbf_12) % 8 ) == 0 );

struct NBFDirectedLight_nbf_12 : public NBFLightSource_nbf_12
{
  float3_t direction; //!< Specifies the direction of the light source.
  PADDING(4);
};
DP_STATIC_ASSERT( ( sizeof(NBFDirectedLight_nbf_12) % 8 ) == 0 );

struct NBFPointLight_nbf_12 : public NBFLightSource_nbf_12
{
  float3_t position;    //!< Specifies the position of the light source.
  float3_t attenuation; //!< Specifies the attenuation factors of the point light.
  //!< The x-component of the vector specifies the constant term of the attenuation,
  //!< the y-component of the vector specifies the linear term of the attenuation, and
  //!< the z-component of the vector specifies the quadratic term of the attenuation.
};
DP_STATIC_ASSERT( ( sizeof(NBFPointLight_nbf_12) % 8 ) == 0 );

struct NBFSpotLight_nbf_12 : public NBFLightSource_nbf_12
{
  float3_t position;        //!< Specifies the position of the light source.
  float3_t direction;       //!< Specifies the direction of the light source.
  float3_t attenuation;     //!< Specifies the attenuation factors of the spot light.
  //!< The x-component of the vector specifies the constant term of the attenuation,
  //!< the y-component of the vector specifies the linear term of the attenuation, and
  //!< the z-component of the vector specifies the quadratic term of the attenuation.
  float    cutoffAngle;     //!< Specifies the angle between the axis of the cone, the light is emitted to, and
  //!< a ray along the edge of the cone.
  float    falloffExponent; //!< Controls the intensity distribution inside the cone, the light is mitted to.
  PADDING(4);               //!< Padding bits to ensure the size of NBFSpotLight is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFSpotLight_nbf_12) % 8 ) == 0 );

struct NBFGroup_nbf_12 : public NBFNode
{
  uint_t      numChildren;        //!< Specifies the number of maintained children.
  uint_t      children;           //!< Specifies the file offset to the offsets to the maintained children.
  //!< NBFGroup's children always are of NBFNode-derived types.
  uint_t      numClipPlanes;      //!< Specifies the number of clipping planes.
  uint_t      clipPlanes;         //!< Specifies the file offset to the clipping planes
};
DP_STATIC_ASSERT( ( sizeof(NBFGroup_nbf_12) % 8 ) == 0 );

struct NBFLOD_nbf_12 : public NBFGroup_nbf_12
{
  float3_t    center;             //!< Specifies the center point used for distance calculations.
  uint_t      numRanges;          //!< Specifies the number of contained ranges.
  uint_t      ranges;             //!< Specifies the file offset to the ranges.
  //!< Ranges are stored as 32-bit floating point numbers.
  PADDING(4);        //!< Padding bits to ensure the size of NBFLOD is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFLOD_nbf_12) % 8 ) == 0 );

struct NBFSwitch_nbf_12 : public NBFGroup_nbf_12
{
  uint_t      numActiveChildren;  //!< Specifies the number of children that are currently switched on.
  uint_t      activeChildren;     //!< Specifies the offset to indices to the active children.
  //!< Indices are stored as 32-bit unsigned integers.
};
DP_STATIC_ASSERT( ( sizeof(NBFSwitch_nbf_12) % 8 ) == 0 );

struct NBFSwitch_nbf_30 : public NBFGroup
{
  uint_t      numActiveChildren;  //!< Specifies the number of children that are currently switched on.
  uint_t      activeChildren;     //!< Specifies the offset to indices to the active children.
  //!< Indices are stored as 32-bit unsigned integers.
};
DP_STATIC_ASSERT( ( sizeof(NBFSwitch_nbf_30) % 8 ) == 0 );

struct NBFBillboard_nbf_12 : public NBFGroup_nbf_12
{
  float3_t  rotationAxis;
  ubyte_t   viewerAligned;
  PADDING(3);        //!< Padding bits to ensure the size of NBFBillboard is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFBillboard_nbf_12) % 8 ) == 0 );

struct NBFTransform_nbf_12 : public NBFGroup_nbf_12
{
  trafo_t             trafo;
  PADDING(4);        //!< Padding bits to ensure the size of NBFTransform is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTransform_nbf_12) % 8 ) == 0 );

struct NBFAnimatedTransform_nbf_12 : public NBFTransform_nbf_12
{
  uint_t      animation;          //!< Specifies the file offset to the NBFTrafoAnimation_nbf_3d object
  //!< to be applied to the transform group node.
  PADDING(4);        //!< Padding bits to ensure the size of NBFAnimatedTransform_nbf_12 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedTransform_nbf_12) % 8 ) == 0 );

struct NBFVertexAttributeSet_nbf_38 : public NBFObject
{
  uint_t      numVertices;        //!< Specifies the number of contained vertices.
  uint_t      vertices;           //!< Specifies the file offset to the vertices. Vertices are stored as float3_t.
  uint_t      numNormals;         //!< Specifies the number of contained normals.
  uint_t      normals;            //!< Specifies the file offset to the normals. Normals are stored as float3_t.
  uint_t      numTexCoordsSets;   //!< Specifies the number of contained texture coordinate sets.
  uint_t      texCoordsSets;      //!< Specifies the file offset to the texture coordinate sets.
  //!< Texture coordinate sets are stored using the texCoordSet_t type.
  uint_t      numColors;          //!< Specifies the number of contained primary colors.
  uint_t      colorDim;           //!< Specifies the dimension, in terms of float, used for the primary colors.
  //!< Colors can be either three or four dimensional.
  uint_t      colors;             //!< Specifies the file offset to the primary colors. In conformity to the color's dimension,
  //!< colors are stored as float3_t or float4_t respectively.
  uint_t      numSecondaryColors; //!< Specifies the number of contained secondary colors.
  uint_t      secondaryColorDim;  //!< Specifies the dimension, in terms of float, used for the secondary colors.
  //!< Colors can be either three or four dimensional.
  uint_t      secondaryColors;    //!< Specifies the file offset to the secondary colors. In conformity to the color's dimension,
  //!< colors are stored as float3_t or float4_t respectively.
  uint_t      numFogCoords;       //!< Specifies the number of contained fog coordinates.
  uint_t      fogCoords;          //!< Specifies the file offset to the fog coordinates. Fog coordinates always are one
  //!< dimensional, and are stored as 32-bit floating point values.
};
DP_STATIC_ASSERT( ( sizeof(NBFVertexAttributeSet_nbf_38) % 8 ) == 0 );

struct NBFViewState_nbf_39
{
  uint_t      objectCode;           //!< Specifies the object code of the actual object. The object code is unique per object type!
  uint_t      camera;               //!< Specifies the offset to the NBFCamera object to be used for viewing.
  ubyte_t     isJitter;             //!< Indicates whether the view is in jitter mode.
  ubyte_t     isStereo;             //!< Indicates whether the view is in stereo mode.
  ubyte_t     isStereoAutomatic;    //!< Indicates whether eye distance is automatically adjusted in stereo mode.
  PADDING(1);                       //!< Padding bits to ensure offset of jitters is on a 4-byte boundary, regardless of packing.
  uint_t      numJitters;           //!< Specifies the number of jitter values available.
  uint_t      jitters;              //!< Specifies the offset to the float2_t jitter values used with jitter mode on.
  float       stereoAutomaticFactor;//!< Specifies the automatic eye distance adjustment factor in stereo mode.
  float       stereoEyeDistance;    //!< Specifies the stereo eye distance used if the view is in stereo mode.
  float       targetDistance;       //!< Specifies the target distance to the projection plane.
};
DP_STATIC_ASSERT( ( sizeof(NBFViewState_nbf_39) % 4 ) == 0 );

/** A NBFAnimatedIndependents_nbf_3a is an abstract topology class derived from NBFIndependentPrimitiveSet.
* The object code for a NBFAnimatedIndependents_nbf_3a is DPBFCode::ANIMATED_QUADS and DPBFCode::ANIMATED_TRIANGLES. */
struct NBFAnimatedIndependents_nbf_3a : public NBFIndependentPrimitiveSet
{
  uint_t      animation;          //!< Specifies the file offset to the NBFVNVectorAnimation object
                                  //!< to be applied to the independent faces.
  PADDING(4);        //!< Padding bits to ensure the size of NBFAnimatedIndependents_nbf_3a is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedIndependents_nbf_3a) % 8 ) == 0 );

//! The NBFVNVectorAnimation_nbf_3a represents an animation that will be applied to VNVector objects.
/** A NBFVNVectorAnimation_nbf_3a serves as base class only and needs to be considered in conjunction
* with either NBFFramedAnimation_nbf_a or NBFInterpolatedAnimation_nbf_3a.\n
* Concrete object codes valid for a NBFVNVectorAnimation_nbf_3a are DPBFCode::FRAMED_VNVECTOR_ANIMATION,
* and DPBFCode::LINEAR_INTERPOLATED_VNVECTOR_ANIMATION. Further concrete object codes valid for
* NBFVNVectorAnimation_nbf_3a objects are subject to future extensions of the NBF format. */
struct NBFVNVectorAnimation_nbf_3a : public NBFObject
{
  uint_t      numVertices;        //!< Specifies the number of vertices per vertex set. Each vertex set of
                                  //!< the actual animation is specified to have the same count of vertices.
  uint_t      vertexSets;         //!< Specifies the file offset to the pre-transformed vertex sets for this animation.
                                  //!< The vertex sets are stored adjoined to each other in contiguous memory.\n
                                  //!< The actual number of vertex sets, as well as normal sets, is specified by
                                  //!< the corresponding animation type, which is either NBFFramedAnimation_nbf_a or
                                  //!< NBFInterpolatedAnimation_nbf_3a.
  uint_t      numNormals;         //!< Specifies the number of normals per normal set. Each normal set of
                                  //!< the actual animation is specified to have the same count of normals.
  uint_t      normalSets;         //!< Specifies the file offset to the pre-transformed normal sets for this animation.
                                  //!< The normal sets are stored adjoined to each other in contiguous memory.\n
                                  //!< The actual number of normal sets, as well as vertex sets, is specified by
                                  //!< the corresponding animation type, which is either NBFFramedAnimation_nbf_a or
                                  //!< NBFInterpolatedAnimation_nbf_3a.
};
DP_STATIC_ASSERT( ( sizeof(NBFVNVectorAnimation_nbf_3a) % 8 ) == 0 );

//! The NBFFramedAnimation_nbf_3c structure represents a framed animation.
/** A NBFFramedAnimation_nbf_3c serves as base class only and needs to be considered in conjunction
  * with either NBFIndexAnimation_nbf_3d, NBFTrafoAnimation_nbf_3d or NBFVertexAttributeAnimation_nbf_3d.\n
  * Concrete object codes valid for a NBFFramedAnimation_nbf_3c are DPBFCode::FRAMED_INDEX_ANIMATION_DESCRIPTION,
  * DPBFCode::FRAMED_TRAFO_ANIMATION_DESCRIPTION, and DPBFCode::FRAMED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. Further concrete object
  * codes valid for NBFFramedAnimation_nbf_3c objects are subject to future extensions of the NBF format. */
struct NBFFramedAnimation_nbf_3c
{
  uint_t      numSteps;           //!< Specifies the number of animation steps.\n
                                  //!< In conjunction with a NBFIndexAnimation_nbf_3d, this corresponds to the number of
                                  //!< uint_t objects stored with a NBFTrafoAnimation_nbf_3d.\n
                                  //!< In conjunction with a NBFTrafoAnimation_nbf_3d, this corresponds to the number of
                                  //!< trafo_t objects stored with a NBFTrafoAnimation_nbf_3d.\n
                                  //!< In conjunction with a NBFVertexAttributeAnimation_nbf_3d, this corresponds to the number of
                                  //!< VertexAttribute sets stored with a NBFVertexAttributeAnimation_nbf_3d.
  PADDING(4);         //!< Padding bits to ensure the size of NBFFramedAnimation_nbf_3c is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedAnimation_nbf_3c) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFIndexAnimation_nbf_3d represents an animation that will be applied to uint_t objects.
/** A NBFIndexAnimation_nbf_3d serves as base class only and needs to be considered in conjunction with
  * NBFFramedAnimationDescription.\n
  * Valid object code for a NBFIndexAnimation_nbf_3d is DPBFCode::FRAMED_INDEX_ANIMATION_DESCRIPTION. Further concrete object
  * codes valid for NBFIndexAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFIndexAnimation_nbf_3d : public NBFObject
{
  uint_t      indices;             //!< Specifies the file offset to the uint_t objects the animation will be applied to.
                                   //!< The actual number of uint_t objects is specified by the corresponding animation
                                   //!< type, which is NBFFramedAnimationDescription.
  PADDING(4);        //!< Padding bits to ensure the size of NBFIndexAnimation_nbf_3d is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFIndexAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedIndexAnimation_nbf_3c structure represents a framed animation that will be applied to uint_t objects.
/** A NBFFramedIndexAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFIndexAnimation_nbf_3d and
  * NBFFramedAnimation_nbf_3c. The object code for a NBFFramedIndexAnimation_nbf_3c is DPBFCode::FRAMED_INDEX_ANIMATION_DESCRIPTION. */
struct NBFFramedIndexAnimation_nbf_3c : public NBFIndexAnimation_nbf_3d
                                      , public NBFFramedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedIndexAnimation_nbf_3c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFTrafoAnimation_nbf_3d represents an animation that will be applied to trafo_t objects.
/** A NBFTrafoAnimation_nbf_3d serves as base class only and needs to be considered in conjunction
* with either NBFFramedAnimationDescription or NBFKeyFramedAnimationDescription.\n
* Valid object codes for a NBFTrafoAnimation_nbf_3d are DPBFCode::FRAMED_TRAFO_ANIMATION_DESCRIPTION,
* and DPBFCode::LINEAR_INTERPOLATED_TRAFO_ANIMATION_DESCRIPTION. Further concrete object codes valid for
* NBFTrafoAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFTrafoAnimation_nbf_3d : public NBFObject
{
  uint_t      trafos;             //!< Specifies the file offset to the trafo_t objects the animation will be applied to.
  //!< The actual number of trafo_t objects is specified by the corresponding animation
  //!< type, which is either NBFFramedAnimationDescription or NBFKeyFramedAnimationDescription.
  PADDING(4);        //!< Padding bits to ensure the size of NBFTrafoAnimation_nbf_3d is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFTrafoAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedTrafoAnimation_nbf_3c structure represents a framed animation that will be applied to trafo_t objects.
/** A NBFFramedTrafoAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFTrafoAnimation_nbf_3d and
  * NBFFramedAnimation_nbf_3c. The object code for a NBFFramedTrafoAnimation_nbf_3c is DPBFCode::FRAMED_TRAFO_ANIMATION_DESCRIPTION. */
struct NBFFramedTrafoAnimation_nbf_3c : public NBFTrafoAnimation_nbf_3d
                                      , public NBFFramedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedTrafoAnimation_nbf_3c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFVertexAttributeAnimation_nbf_3d represents an animation that will be applied to vertexAttrib_t objects.
/** A NBFVertexAttributeAnimation_nbf_3d serves as base class only and needs to be considered in conjunction
  * with either NBFFramedAnimationDescription or NBFKeyFramedAnimationDescription.\n
  * Valid object codes for a NBFVertexAttributeAnimation_nbf_3d are DPBFCode::FRAMED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION,
  * and DPBFCode::LINEAR_INTERPOLATED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. Further concrete object codes valid for
  * NBFVertexAttributeAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFVertexAttributeAnimation_nbf_3d : public NBFObject
{
  uint_t      attribs;            //!< Specifies the file offset to the vertexAttrib_t objects the animation will be applied to.
                                  //!< The actual number of vertexAttrib_t objects is specified by the corresponding animation
                                  //!< type, which is either NBFFramedAnimationDescription or NBFKeyFramedAnimationDescription.
  PADDING(4);        //!< Padding bits to ensure the size of NBFVertexAttributeAnimation_nbf_3d is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFVertexAttributeAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedVertexAttributeAnimation_nbf_3c structure represents a framed animation that will be applied to vertexAttrib_t objects.
/** A NBFFramedVertexAttributeAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFVertexAttributeAnimation_nbf_3d and
* NBFFramedAnimation_nbf_3c. The object code for a NBFFramedVertexAttributeAnimation_nbf_3c is DPBFCode::FRAMED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. */
struct NBFFramedVertexAttributeAnimation_nbf_3c : public NBFVertexAttributeAnimation_nbf_3d
                                                , public NBFFramedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedVertexAttributeAnimation_nbf_3c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFInterpolatedAnimation_nbf_3c structure represents a interpolated animation.
/** A NBFInterpolatedAnimation_nbf_3c serves as base class only and needs to be considered in conjunction
  * with either NBFTrafoAnimation_nbf_3d or NBFVertexAttributeAnimation_nbf_3d.\n
  * Concrete object codes valid for a NBFInterpolatedAnimation_nbf_3c are
  * DPBFCode::LINEAR_INTERPOLATED_TRAFO_ANIMATION_DESCRIPTION and DPBFCode::LINEAR_INTERPOLATED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION.
  * Further concrete object codes valid for NBFInterpolatedAnimation_nbf_3c objects are subject to future
  * extensions of the NBF format. */
struct NBFInterpolatedAnimation_nbf_3c
{
  uint_t      numKeys;            //!< Specifies the number of key frames
  uint_t      keys;               //!< Specifies the file offset to the key frames. As specified in the NBF format,
  //!< a key is a 32-bit unsigned integer value.
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedAnimation_nbf_3c) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFInterpolatedTrafoAnimation_nbf_3c structure represents a interpolated animation that will be applied to trafo_t objects.
/** A NBFInterpolatedTrafoAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFTrafoAnimation_nbf_3d and
  * NBFInterpolatedAnimation_nbf_3c.\n
  * The object code valid for a NBFInterpolatedTrafoAnimation_nbf_3c is DPBFCode::LINEAR_INTERPOLATED_TRAFO_ANIMATION_DESCRIPTION. Further object codes
  * valid for NBFInterpolatedTrafoAnimation_nbf_3c objects are subject to future extensions of the NBF format. */
struct NBFInterpolatedTrafoAnimation_nbf_3c : public NBFTrafoAnimation_nbf_3d
                                            , public NBFInterpolatedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedTrafoAnimation_nbf_3c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFInterpolatedVertexAttributeAnimation_nbf_3c structure represents a interpolated animation that will be applied to vertexAttrib_t objects.
/** A NBFInterpolatedVertexAttributeAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFVertexAttributeAnimation_nbf_3d and
  * NBFInterpolatedAnimation_nbf_3c.\n
  * The object code valid for a NBFInterpolatedVertexAttributeAnimation_nbf_3c is DPBFCode::LINEAR_INTERPOLATED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. Further object codes
  * valid for NBFInterpolatedVertexAttributeAnimation_nbf_3c objects are subject to future extensions of the NBF format. */
struct NBFInterpolatedVertexAttributeAnimation_nbf_3c : public NBFVertexAttributeAnimation_nbf_3d
                                                      , public NBFInterpolatedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedVertexAttributeAnimation_nbf_3c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedVNVectorAnimation_nbf_3a structure represents a framed animation that will be applied to VNVector objects.
/** A NBFFramedVNVectorAnimation_nbf_3a is a concrete animation type. It publicly inherits from NBFVNVectorAnimation and
* NBFFramedAnimation_nbf_3a. The object code for a NBFFramedVNVectorAnimation_nbf_3a is DPBFCode::FRAMED_VNVECTOR_ANIMATION. */
struct NBFFramedVNVectorAnimation_nbf_3a : public NBFVNVectorAnimation_nbf_3a
                                         , public NBFFramedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedVNVectorAnimation_nbf_3a) % 8 ) == 0 );

//! The NBFInterpolatedVNVectorAnimation_nbf_3a structure represents a interpolated animation that will be applied to VNVector objects.
/** A NBFInterpolatedVNVectorAnimation_nbf_3a is a concrete animation type. It publicly inherits from NBFVNVectorAnimation and
* NBFInterpolatedAnimation_nbf_3a.\n
* The object code valid for a NBFInterpolatedVNVectorAnimation_nbf_3a is DPBFCode::LINEAR_INTERPOLATED_VNVECTOR_ANIMATION. Further object codes
* valid for NBFInterpolatedVNVectorAnimation_nbf_3a objects are subject to future extensions of the NBF format. */
struct NBFInterpolatedVNVectorAnimation_nbf_3a : public NBFVNVectorAnimation_nbf_3a
                                               , public NBFInterpolatedAnimation_nbf_3c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedVNVectorAnimation_nbf_3a) % 8 ) == 0 );

struct NBFNode_3d_04 : public NBFObject
{
  str_t       annotation;         //!< Specifies an optional annotation string. Unused since v61.2!
  PADDING(6);                     //!< Padding bits to ensure the size of NBFStateAttribute is a multiple of 4, regardless of packing.
  ubyte_t     systemHints;        //!< Specifies the system hints on this Node.
  ubyte_t     userHints;          //!< Specifies the user hints on this Node.
};
DP_STATIC_ASSERT( ( sizeof(NBFNode_3d_04) % 8 ) == 0 );   //!< Compile-time assert on size of structure
DP_STATIC_ASSERT( sizeof(NBFNode_3d_04) == sizeof(NBFNode) );

//! The NBFObject structure represents general object data.
struct NBFObject_3d_04
{
  uint_t      objectCode;         //!< Specifies the object code of the actual object. The object code is unique per object type!
  ubyte_t     isShared;           //!< Indicates whether the data of the actual object is shared among different objects.
                                  //!< A value of 1 indicates that this object's data is shared, whereas a value of 0 indicates
                                  //!< that this object's data is not shared.
  PADDING(3);                     //!< pad so everything is 32 bit aligned

  uint64_t    objectDataID;       //!< A unique 64-bit value to identify shared object data while loading.
  uint_t      sourceObject;       //!< Specifies the file offset to the source object in case of data sharing.
                                  //!< A file offset of 0 always indicates that no source object is available for the actual object.
  uint_t       objectName;        //!< Specifies the offset to an optional name. A 0-offset implies no name.
                                  //!< The name is stored as a str_t object.
  PADDING(2);                     //!< Padding bits to keep compatibility to earlier versions.
  uint_t       objectAnno;        //!< Specifies the offset to an optional annotation that can be specified for an object.
                                  //!< A 0-offset implies no annotation. An annotation is stored as a str_t object.
  PADDING(2);                     //!< Padding bits to keep compatibility to earlier versions.
};
DP_STATIC_ASSERT( ( sizeof(NBFObject_3d_04) % 8 ) == 0 );   //!< Compile-time assert on size of structure
DP_STATIC_ASSERT( sizeof(NBFObject_3d_04) == sizeof(NBFObject) );

//! The NBFAnimation_nbf_3d structure represents the common anchor of all animation structures.
/** A NBFAnimation_nbf_3d serves as a base class only and needs to be considered in conjunction with either
* NBFFramedAnimation_nbf_3d or NBFInterpolatedAnimation_nbf_3d. */
struct NBFAnimation_nbf_3d
{
  uint_t  loopCount;              //!< Specifies the number of times to run through the Animation.
  uint_t  speed;                  //!< Specifies the number if steps per advance.
  bool    forward;                //!< Specifies to run the Animation forward or backward.
  bool    swinging;               //!< Specifies to run the Animation looping or swinging (ping-pong).
  PADDING(6);        //!< Padding bits to ensure the size of NBFAnimation_nbf_3d is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedAnimation_nbf_3d structure represents a framed animation.
/** A NBFFramedAnimation_nbf_3d serves as base class only and needs to be considered in conjunction
* with either NBFIndexAnimation_nbf_3d, NBFTrafoAnimation_nbf_3d or NBFVertexAttributeAnimation_nbf_3d.\n
* Concrete object codes valid for a NBFFramedAnimation_nbf_3d are DPBFCode::FRAMED_INDEX_ANIMATION_DESCRIPTION,
* DPBFCode::FRAMED_TRAFO_ANIMATION_DESCRIPTION, and DPBFCode::FRAMED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. Further concrete object
* codes valid for NBFFramedAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFFramedAnimation_nbf_3d : public NBFAnimation_nbf_3d
{
  uint_t      numSteps;           //!< Specifies the number of animation steps.\n
  //!< In conjunction with a NBFIndexAnimation_nbf_3d, this corresponds to the number of
  //!< uint_t objects stored with a NBFTrafoAnimation_nbf_3d.\n
  //!< In conjunction with a NBFTrafoAnimation_nbf_3d, this corresponds to the number of
  //!< trafo_t objects stored with a NBFTrafoAnimation_nbf_3d.\n
  //!< In conjunction with a NBFVertexAttributeAnimation_nbf_3d, this corresponds to the number of
  //!< VertexAttribute sets stored with a NBFVertexAttributeAnimation_nbf_3d.
  PADDING(4);         //!< Padding bits to ensure the size of NBFFramedAnimation_nbf_3d is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedAnimation_nbf_3d) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFInterpolatedAnimation_nbf_3d structure represents a interpolated animation.
/** A NBFInterpolatedAnimation_nbf_3d serves as base class only and needs to be considered in conjunction
* with either NBFTrafoAnimation_nbf_3d or NBFVertexAttributeAnimation_nbf_3d.\n
* Concrete object codes valid for a NBFInterpolatedAnimation_nbf_3d are
* DPBFCode::LINEAR_INTERPOLATED_TRAFO_ANIMATION_DESCRIPTION and DPBFCode::LINEAR_INTERPOLATED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION.
* Further concrete object codes valid for NBFInterpolatedAnimation_nbf_3d objects are subject to future
* extensions of the NBF format. */
struct NBFInterpolatedAnimation_nbf_3d : public NBFAnimation_nbf_3d
{
  uint_t      numKeys;            //!< Specifies the number of key frames
  uint_t      keys;               //!< Specifies the file offset to the key frames. As specified in the NBF format,
  //!< a key is a 32-bit unsigned integer value.
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedAnimation_nbf_3d) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFFramedIndexAnimation_nbf_3d structure represents a framed animation that will be applied to uint_t objects.
/** A NBFFramedIndexAnimation_nbf_3d is a concrete animation type. It publicly inherits from NBFIndexAnimation_nbf_3d and
* NBFFramedAnimation_nbf_3d. The object code for a NBFFramedIndexAnimation_nbf_3d is DPBFCode::FRAMED_INDEX_ANIMATION_DESCRIPTION. */
struct NBFFramedIndexAnimation_nbf_3d : public NBFIndexAnimation_nbf_3d
                                      , public NBFFramedAnimation_nbf_3d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedIndexAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedTrafoAnimation_nbf_3d structure represents a framed animation that will be applied to trafo_t objects.
/** A NBFFramedTrafoAnimation_nbf_3d is a concrete animation type. It publicly inherits from NBFTrafoAnimation_nbf_3d and
* NBFFramedAnimation_nbf_3d. The object code for a NBFFramedTrafoAnimation_nbf_3d is DPBFCode::FRAMED_TRAFO_ANIMATION_DESCRIPTION. */
struct NBFFramedTrafoAnimation_nbf_3d : public NBFTrafoAnimation_nbf_3d
                                      , public NBFFramedAnimation_nbf_3d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedTrafoAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFramedVertexAttributeAnimation_nbf_3c structure represents a framed animation that will be applied to vertexAttrib_t objects.
/** A NBFFramedVertexAttributeAnimation_nbf_3c is a concrete animation type. It publicly inherits from NBFVertexAttributeAnimation_nbf_3d and
* NBFFramedAnimation_nbf_3d. The object code for a NBFFramedVertexAttributeAnimation_nbf_3c is DPBFCode::FRAMED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. */
struct NBFFramedVertexAttributeAnimation_nbf_3d : public NBFVertexAttributeAnimation_nbf_3d
                                                , public NBFFramedAnimation_nbf_3d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFFramedVertexAttributeAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFInterpolatedTrafoAnimation_nbf_3d structure represents a interpolated animation that will be applied to trafo_t objects.
/** A NBFInterpolatedTrafoAnimation_nbf_3d is a concrete animation type. It publicly inherits from NBFTrafoAnimation_nbf_3d and
* NBFInterpolatedAnimation_nbf_3d.\n
* The object code valid for a NBFInterpolatedTrafoAnimation_nbf_3d is DPBFCode::LINEAR_INTERPOLATED_TRAFO_ANIMATION_DESCRIPTION. Further object codes
* valid for NBFInterpolatedTrafoAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFInterpolatedTrafoAnimation_nbf_3d : public NBFTrafoAnimation_nbf_3d
                                            , public NBFInterpolatedAnimation_nbf_3d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedTrafoAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFInterpolatedVertexAttributeAnimation_nbf_3d structure represents a interpolated animation that will be applied to vertexAttrib_t objects.
/** A NBFInterpolatedVertexAttributeAnimation_nbf_3d is a concrete animation type. It publicly inherits from NBFVertexAttributeAnimation_nbf_3d and
* NBFInterpolatedAnimation_nbf_3d.\n
* The object code valid for a NBFInterpolatedVertexAttributeAnimation_nbf_3d is DPBFCode::LINEAR_INTERPOLATED_VERTEX_ATTRIBUTE_ANIMATION_DESCRIPTION. Further object codes
* valid for NBFInterpolatedVertexAttributeAnimation_nbf_3d objects are subject to future extensions of the NBF format. */
struct NBFInterpolatedVertexAttributeAnimation_nbf_3d : public NBFVertexAttributeAnimation_nbf_3d
                                                      , public NBFInterpolatedAnimation_nbf_3d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFInterpolatedVertexAttributeAnimation_nbf_3d) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFMaterial_nbf_3f structure represents a material.
/** The object code for a NBFMaterial_nbf_3f is DPBFCode::MATERIAL. */
struct NBFMaterial_nbf_3f : public NBFObject
{
  float3_t    frontAmbientColor;      //!< Specifies the ambient part of the front material color.
  float3_t    frontDiffuseColor;      //!< Specifies the diffuse part of the front material color.
  float3_t    frontEmissiveColor;     //!< Specifies the emissive part of the front material color.
  float       frontOpacity;           //!< Specifies the opacity of the front material.
  float3_t    frontSpecularColor;     //!< Specifies the specular part of the front material color.
  float       frontSpecularExponent;  //!< Specifies the specular exponent of the front material color.
  float3_t    backAmbientColor;       //!< Specifies the ambient part of the back material color.
  float3_t    backDiffuseColor;       //!< Specifies the diffuse part of the back material color.
  float3_t    backEmissiveColor;      //!< Specifies the emissive part of the back material color.
  float       backOpacity;            //!< Specifies the opacity of the back material.
  float3_t    backSpecularColor;      //!< Specifies the specular part of the back material color.
  float       backSpecularExponent;   //!< Specifies the specular exponent of the back material color.
};
DP_STATIC_ASSERT( ( sizeof(NBFMaterial_nbf_3f) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFMaterial_nbf_40 structure represents a material.
/** The object code for a NBFMaterial_nbf_40 is DPBFCode::MATERIAL. */
struct NBFMaterial_nbf_40 : public NBFObject
{
  float3_t    frontAmbientColor;      //!< Specifies the ambient part of the front material color.
  float3_t    frontDiffuseColor;      //!< Specifies the diffuse part of the front material color.
  float3_t    frontEmissiveColor;     //!< Specifies the emissive part of the front material color.
  float       frontIndexOfRefraction; //!< Specifies the index of refraction of the front material.
  float       frontOpacity;           //!< Specifies the opacity of the front material.
  float3_t    frontReflectiveColor;   //!< Specifies the reflective color of the front material.
  float       frontReflectivity;      //!< Specifies the reflectivity of the front material.
  float3_t    frontSpecularColor;     //!< Specifies the specular part of the front material color.
  float       frontSpecularExponent;  //!< Specifies the specular exponent of the front material color.
  float3_t    frontTransparentColor;  //!< Specifies the transparent color of the front material.
  float3_t    backAmbientColor;       //!< Specifies the ambient part of the back material color.
  float3_t    backDiffuseColor;       //!< Specifies the diffuse part of the back material color.
  float3_t    backEmissiveColor;      //!< Specifies the emissive part of the back material color.
  float       backIndexOfRefraction;  //!< Specifies the index of refraction of the back material.
  float       backOpacity;            //!< Specifies the opacity of the back material.
  float3_t    backReflectiveColor;    //!< Specifies the reflective color of the back material.
  float       backReflectivity;       //!< Specifies the reflectivity of the back material.
  float3_t    backSpecularColor;      //!< Specifies the specular part of the back material color.
  float       backSpecularExponent;   //!< Specifies the specular exponent of the back material color.
  float3_t    backTransparentColor;   //!< Specifies the transparent color of the back material.
};
DP_STATIC_ASSERT( ( sizeof(NBFMaterial_nbf_40) % 8 ) == 0 );   //!< Compile-time assert on size of structure

struct NBFCamera_nbf_44 : public NBFObject
{
  uint_t      numHeadLights;    //!< Specifies the number of headlights attached.
  uint_t      headLights;       //!< Specifies the file offset to the offsets to the attached headlight objects.
                                //!< Headlights are of type NBFLightSource.
  float3_t    upVector;         //!< Specifies the camera's normalized up vector.
  float3_t    position;         //!< Specifies the actual position of camera in world space.
  float3_t    direction;        //!< Specifies the normalized direction for the camera to look along.
  float       farDist;          //!< Specifies the distance from the actual camera position to the far clipping plane.
  float       nearDist;         //!< Specifies the distance from the actual camera position to the near clipping plane.
  float       focusDist;        //!< Specifies the distance to the projection plane.
  ubyte_t     isAutoClipPlanes; //!< Indicates if automatic generation of clipping planes is enabled.
  PADDING(3);      //!< Padding bits to ensure offset of windowSize is on a 4-byte boundary, regardless of packing
  float2_t    windowSize;       //!< Specifies the world-relative size of the viewing window. Whereas the x-component of
                                //!< of the vector specifies the width, and the y-component of the vector specifies the height.
  float2_t    windowOffset;     //!< Specifies the world-relative offset from the viewing reference point to the center
                                //!< of the viewing window.
  PADDING(4);        //!< Padding bits to ensure the size of NBFCamera is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFCamera_nbf_44) % 8 ) == 0 );   //!< Compile-time assert on size of structure

struct NBFParallelCamera_nbf_44 : public NBFCamera_nbf_44
{
};
DP_STATIC_ASSERT( ( sizeof(NBFParallelCamera_nbf_44) % 8 ) == 0 );   //!< Compile-time assert on size of structure

struct NBFPerspectiveCamera_nbf_44 : public NBFCamera_nbf_44
{
};
DP_STATIC_ASSERT( ( sizeof(NBFPerspectiveCamera_nbf_44) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The texImage_t structure specifies how a texture image is stored in a .DPBF file.
/** Texture images are considered in conjunction with NBFTextureAttributeItem objects. */
struct texImage_nbf_4b_t
{
  uint_t      flags;              //!< Creation flags.
  str_t       file;               //!< Specifies the filename of the image file in case the image is from a file.
  // the following are only relevant in case the image is not from a file but from a image data lump.
  uint_t      width;              //!< Specifies the width of the texture in pixels.
  uint_t      height;             //!< Specifies the height of the texture in pixels.
  uint_t      depth;              //!< Specifies the depth of the texture in pixels.
  PADDING(12);                     //!< Padding bits to ensure offset of scene is on a 4-byte boundary, regardless of packing.
  uint_t      pixelFormat;        //!< Specifies the format of the pixel data.
  uint_t      dataType;           //!< Specifies the type of the pixel data.
  uint_t      pixels;             //!< Specifies the file offset to the raw pixel data.
};
DP_STATIC_ASSERT( ( sizeof(texImage_nbf_4b_t) % 4 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFFrustumCamera_nbf_4c : public NBFCamera
{
  float       farDist;          //!< Specifies the distance from the actual camera position to the far clipping plane.
  ubyte_t     isAutoClipPlanes; //!< Indicates if automatic generation of clipping planes is enabled.
  PADDING(3);      //!< Padding bits to ensure offset of windowSize is on a 4-byte boundary, regardless of packing
  float       nearDist;         //!< Specifies the distance from the actual camera position to the near clipping plane.
  float2_t    windowOffset;     //!< Specifies the world-relative offset from the viewing reference point to the center
                                //!< of the viewing window.
  float2_t    windowSize;       //!< Specifies the world-relative size of the viewing window. Whereas the x-component of
                                //!< of the vector specifies the width, and the y-component of the vector specifies the height.
  PADDING(4);        //!< Padding bits to ensure the size of NBFCamera is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFFrustumCamera_nbf_4c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFParallelCamera_nbf_4c represents a parallel camera.
/** A NBFParallelCamera_nbf_4c is a concrete camera type.
  * The object code for a NBFParallelCamera_nbf_4c is DPBFCode::PARALLEL_CAMERA. */
struct NBFParallelCamera_nbf_4c : public NBFFrustumCamera_nbf_4c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFParallelCamera_nbf_4c) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFPerspectiveCamera_nbf_4c represents a perspective camera.
/** A NBFPerspectiveCamera_nbf_4c is a concrete camera type.
  * The object code for a NBFPerspectiveCamera_nbf_4c is DPBFCode::PERSPECTIVE_CAMERA. */
struct NBFPerspectiveCamera_nbf_4c : public NBFFrustumCamera_nbf_4c
{
};
DP_STATIC_ASSERT( ( sizeof(NBFPerspectiveCamera_nbf_4c) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The indexSet_t structure specifies how primitive indices are stored in a .DPBF file
/** Objects of type indexSet_t are always considered in conjunction with NBFPrimitive_nbf_4d objects. */
struct indexSet_t
{
  uint_t      dataType;               //!< Data type
  uint_t      primitiveRestartIndex;  //!< Primitive Restart Index
  uint_t      numberOfIndices;        //!< Number of indices in buffer
  uint_t      idata;                  //!< the index data
};
DP_STATIC_ASSERT( ( sizeof(indexSet_t) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFPrimitive_nbf_4d structure represents a geometry with an NBFVertexAttributeSet, and possibly an index set
/** A NBFPrimitive_nbf_4d holds the offset to an NBFVertexAttributeSet, and possibly an index set */
struct NBFPrimitive_nbf_4d : public NBFObject
{
  uint_t      primitiveType;      //!< Specified the primitive type (unit because it may be a user enum)
  uint_t      elementOffset;      //!< Specifies the element offset
  uint_t      elementCount;       //!< Specifies the element count
  uint_t      instanceCount;      //!< Specified the instance count
  uint_t      verticesPerPatch;   //!< Specified the num verts per patch
  uint_t      renderFlags;        //!< Specified the rendering flags
  uint_t      vertexAttributeSet; //!< Specifies the file offset to the vertex attribute set.
  indexSet_t  indexSet;           //!< Specifies the index set, if any
  uint_t      skin;               //!< Specifies the file offset to the skin
};
DP_STATIC_ASSERT( ( sizeof(NBFPrimitive_nbf_4d) % 8 ) == 0 );   // Compile-time assert on size of structure

/*! \brief The NBFPatchesBase_nbf_4d structure is the base of all patches structures. */
struct NBFPatchesBase_nbf_4d : public NBFPrimitive_nbf_4d
{
  // nothing required here any more
};
DP_STATIC_ASSERT( ( sizeof(NBFPatchesBase_nbf_4d) % 8 ) == 0 );        //!< Compile-time assert on size of structure

/*! \brief The NBFQuadPatches_nbf_4d structure represents a general quad patches object.
 *  \remarks The object code for a NBFQuadPatches_nbf_4d is DPBFCode::QUAD_PATCHES. */
struct NBFQuadPatches_nbf_4d : public NBFPatchesBase_nbf_4d
{
  uint_t  size;                     //!< Specifies the size of the quad patches. Each patch is specified by size^2 vertices
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches_nbf_4d) % 8 ) == 0 );        //!< Compile-time assert on size of structure

/*! \brief The NBFQuadPatches4x4_nbf_4d structure represents a 4x4 quad patches object.
 *  \remarks The object code for a NBFQuadPatches4x4_nbf_4d is DPBFCode::QUAD_PATCHES_4X4. */
struct NBFQuadPatches4x4_nbf_4d : public NBFPatchesBase_nbf_4d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches4x4_nbf_4d) % 8 ) == 0 );     // Compile-time assert on size of structure

/*! \brief The NBFRectPatches_nbf_4d structure represents a general rectangular patches object.
 *  \remarks The object code for a NBFRectPatches_nbf_4d is DPBFCode::RECT_PATCHES. */
struct NBFRectPatches_nbf_4d : public NBFPatchesBase_nbf_4d
{
  uint_t  width;                  //!< Specifies the width of the patches. Each patch is specified by with*height vertices.
  uint_t  height;                 //!< Specifies the height of the patches. Each patch is specified by width*height vertices.
};
DP_STATIC_ASSERT( ( sizeof(NBFRectPatches_nbf_4d) % 8 ) == 0 );      //!< Compile-time assert on size of structure

/*! \brief The NBFRectPatches_nbf_4d structure represents a general triangular patches object.
 *  \remarks The object code for a NBFRectPatches_nbf_4d is DPBFCode::TRI_PATCHES. */
struct NBFTriPatches_nbf_4d : public NBFPatchesBase_nbf_4d
{
  uint_t  size;                     //!< Specifies the size of the patches. Each patch is specified by 1+2+...+size vertices
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches_nbf_4d) % 8 ) == 0 );     //!< Compile-time assert on size of structure

/*! \brief The NBFTriPatches4_nbf_4d structure represents a 4-vertices-per-edge triangular patches object.
 *  \remarks The object code for a NBFTriPatches4_nbf_4d is DPBFCode::TRI_PATCHES_4. */
struct NBFTriPatches4_nbf_4d : public NBFPatchesBase_nbf_4d
{
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches4_nbf_4d) % 8 ) == 0 );      // Compile-time assert on size of structure

//! The geometrySet_t structure specifies how a geometry set is stored in a .DPBF file.
/** Geometry sets, in this context, need to be considered in conjunction with NBFGeoNode_nbf_4e objects. */
struct geometrySet_t
{
  uint_t      stateSet;           //!< Specifies the file offset to the corresponding NBFStateSet object.
  uint_t      numPrimitives;      //!< Specifies the number of this geometry's NBFPrimitive objects.
  uint_t      primitives;         //!< Specifies the file offset to the offsets to the NBFPrimitive objects.
};
DP_STATIC_ASSERT( ( sizeof(geometrySet_t) % 4 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFGeoNode_nbf_4e structure represents a geometry node.
/** The object code for a NBFGeoNode_nbf_4e is DPBFCode::GEO_NODE. */
struct NBFGeoNode_nbf_4e : public NBFNode
{
  uint_t      numStateSets;       //!< Specifies the number of contained StateSets.
  uint_t      geometrySets;       //!< Specifies the file offset to the geometry sets. Geometry sets are stored as geometrySet_t objects.
};
DP_STATIC_ASSERT( ( sizeof(NBFGeoNode_nbf_4e) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFStatePass_nbf_4f structure represents a set of heterogeneous NBFStateAttributes.
/** The object code for a NBFStatePass_nbf_4f is DPBFCode::STATE_PASS. */
struct NBFStatePass_nbf_4f : public NBFObject
{
  uint_t      numStateAttribs;    //!< Specifies the number of contained state attributes.
  uint_t      stateAttribs;       //!< Specifies the file offset to the offsets to the NBFStateAttribute objects
};
DP_STATIC_ASSERT( ( sizeof(NBFStatePass_nbf_4f) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFStateSet_nbf_4f structure represents a set of pairs of VariantKeys and StateVariant_nbf_4f.
/** The object code for a NBFStateSet_nbf_4f is DPBFCode::STATE_SET. */
struct NBFStateSet_nbf_4f : public NBFObject
{
  uint_t activeKey;               //!< Specifies the currently active VariantKey
  uint_t numStateVariants;        //!< Specifies the number of contained pairs of VariantKey and StateVariant.
  uint_t keyStateVariantPairs;    //!< Specifies the file offset to the offsets to the keyVariant_t objects
  PADDING(4);               //!< Padding bits to ensure the size of NBFStateSet is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFStateSet_nbf_4f) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFStateVariant_nbf_4f structure represents a set of StatePasses_nbf_4f
/** The object code for a NBFStateVariant_nbf_4f is DPBFCode::STATE_VARIANT. */
struct NBFStateVariant_nbf_4f : public NBFObject
{
  uint_t      numStatePasses;     //!< Specifies the number of contained state passes.
  uint_t      statePasses;        //!< Specifies the file offset to the offsets to the NBFStatePass objects
};
DP_STATIC_ASSERT( ( sizeof(NBFStateVariant_nbf_4f) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFLightSource_nbf_50 structure represents a light source node.
/** A NBFLightSource_nbf_50 serves as base class only. Concrete object codes valid for
  * a NBFLightSource_nbf_50 are DPBFCode::DIRECTED_LIGHT, DPBFCode::POINT_LIGHT, and DPBFCode::SPOT_LIGHT. */
struct NBFLightSource_nbf_50 : public NBFObject
{
  float       intensity;          //!< Specifies the light intensity.
  float3_t    ambientColor;       //!< Specifies the ambient color term of the light.
  float3_t    diffuseColor;       //!< Specifies the diffuse color term of the light.
  float3_t    specularColor;      //!< Specifies the specular color term of the light.
  ubyte_t     castShadow;         //!< flag that determines if this light source creates shadows.
  ubyte_t     enabled;            //!< flag to indicate enabled state.
  PADDING(2);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
  uint_t      animation;          //!< Specifies the file offset to an optional NBFTrafoAnimation object
                                  //!< to be applied to the light transform (orientation and translation).
};
DP_STATIC_ASSERT( ( sizeof(NBFLightSource_nbf_50) % 8 ) == 0 );    //!< Compile-time assert on size of structure

// The NBFDirectedLight_nbf_50 structure represents a directed light source.
/** The object code for a NBFDirectedLight_nbf_50 is DPBFCode::DIRECTED_LIGHT. */
struct NBFDirectedLight_nbf_50 : public NBFLightSource_nbf_50
{
  float3_t direction; //!< Specifies the direction of the light source.
  PADDING(4);         //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
};
DP_STATIC_ASSERT( ( sizeof(NBFDirectedLight_nbf_50) % 8 ) == 0 );    //!< Compile-time assert on size of structure

// The NBFPointLight_nbf_50 structure represents a point light source.
/** The object code for a NBFPointLight_nbf_50 is DPBFCode::POINT_LIGHT. */
struct NBFPointLight_nbf_50 : public NBFLightSource_nbf_50
{
  float3_t position;    //!< Specifies the position of the light source.
  float3_t attenuation; //!< Specifies the attenuation factors of the point light.
                        //!< The x-component of the vector specifies the constant term of the attenuation,
                        //!< the y-component of the vector specifies the linear term of the attenuation, and
                        //!< the z-component of the vector specifies the quadratic term of the attenuation.
};
DP_STATIC_ASSERT( ( sizeof(NBFPointLight_nbf_50) % 8 ) == 0 );   //!< Compile-time assert on size of structure

// The NBFSpotLight_nbf_50 structure represents a spot light.
/** The object code for a NBFSpotLight_nbf_50 is DPBFCode::SPOT_LIGHT. */
struct NBFSpotLight_nbf_50 : public NBFLightSource_nbf_50
{
  float3_t position;        //!< Specifies the position of the light source.
  float3_t direction;       //!< Specifies the direction of the light source.
  float3_t attenuation;     //!< Specifies the attenuation factors of the spot light.
                            //!< The x-component of the vector specifies the constant term of the attenuation,
                            //!< the y-component of the vector specifies the linear term of the attenuation, and
                            //!< the z-component of the vector specifies the quadratic term of the attenuation.
  float    cutoffAngle;     //!< Specifies the angle between the axis of the cone, the light is emitted to, and
                            //!< a ray along the edge of the cone.
  float    falloffExponent; //!< Controls the intensity distribution inside the cone, the light is mitted to.
  PADDING(4);               //!< Padding bits to ensure the size of NBFSpotLight is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFSpotLight_nbf_50) % 8 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFGeoNode_nbf_51 : public NBFNode
{
  uint_t      stateSet;           //!< Specifies the file offset to the corresponding NBFStateSet object.
  uint_t      primitive;          //!< Specifies the file offset to the corresponding NBFPrimitive objects.
};
DP_STATIC_ASSERT( ( sizeof(NBFGeoNode_nbf_51) % 8 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFLightSource_nbf_52 : public NBFNode
{
  float       intensity;          //!< Specifies the light intensity.
  float3_t    ambientColor;       //!< Specifies the ambient color term of the light.
  float3_t    diffuseColor;       //!< Specifies the diffuse color term of the light.
  float3_t    specularColor;      //!< Specifies the specular color term of the light.
  ubyte_t     castShadow;         //!< flag that determines if this light source creates shadows.
  ubyte_t     enabled;            //!< flag to indicate enabled state.
  PADDING(2);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
  uint_t      animation;          //!< Specifies the file offset to an optional NBFTrafoAnimation object
                                  //!< to be applied to the light transform (orientation and translation).
};
DP_STATIC_ASSERT( ( sizeof(NBFLightSource_nbf_52) % 8 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFDirectedLight_nbf_52 : public NBFLightSource_nbf_52
{
  float3_t direction; //!< Specifies the direction of the light source.
  PADDING(4);         //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
};
DP_STATIC_ASSERT( ( sizeof(NBFDirectedLight_nbf_52) % 8 ) == 0 );    //!< Compile-time assert on size of structure

struct NBFPointLight_nbf_52 : public NBFLightSource_nbf_52
{
  float3_t position;    //!< Specifies the position of the light source.
  float3_t attenuation; //!< Specifies the attenuation factors of the point light.
                        //!< The x-component of the vector specifies the constant term of the attenuation,
                        //!< the y-component of the vector specifies the linear term of the attenuation, and
                        //!< the z-component of the vector specifies the quadratic term of the attenuation.
};
DP_STATIC_ASSERT( ( sizeof(NBFPointLight_nbf_52) % 8 ) == 0 );   //!< Compile-time assert on size of structure

struct NBFSpotLight_nbf_52 : public NBFLightSource_nbf_52
{
  float3_t position;        //!< Specifies the position of the light source.
  float3_t direction;       //!< Specifies the direction of the light source.
  float3_t attenuation;     //!< Specifies the attenuation factors of the spot light.
                            //!< The x-component of the vector specifies the constant term of the attenuation,
                            //!< the y-component of the vector specifies the linear term of the attenuation, and
                            //!< the z-component of the vector specifies the quadratic term of the attenuation.
  float    cutoffAngle;     //!< Specifies the angle between the axis of the cone, the light is emitted to, and
                            //!< a ray along the edge of the cone.
  float    falloffExponent; //!< Controls the intensity distribution inside the cone, the light is mitted to.
  PADDING(4);               //!< Padding bits to ensure the size of NBFSpotLight is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFSpotLight_nbf_52) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFLightSource_nbf_53 structure represents a light source node.
/** A NBFLightSource_nbf_53 serves as base class only. Concrete object codes valid for
  * a NBFLightSource_nbf_53 are DPBFCode::DIRECTED_LIGHT, DPBFCode::POINT_LIGHT, and DPBFCode::SPOT_LIGHT. */
struct NBFLightSource_nbf_53 : public NBFNode
{
  float       intensity;          //!< Specifies the light intensity.
  float3_t    ambientColor;       //!< Specifies the ambient color term of the light.
  float3_t    diffuseColor;       //!< Specifies the diffuse color term of the light.
  float3_t    specularColor;      //!< Specifies the specular color term of the light.
  ubyte_t     castShadow;         //!< flag that determines if this light source creates shadows.
  ubyte_t     enabled;            //!< flag to indicate enabled state.
  PADDING(2);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
  uint_t      animation;          //!< Specifies the file offset to an optional NBFTrafoAnimation object
                                  //!< to be applied to the light transform (orientation and translation).
  uint_t      lightEffect;        //!< Specifies the file offset to an optional NBFEffectDatat
  PADDING(4);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
};
DP_STATIC_ASSERT( ( sizeof(NBFLightSource_nbf_53) % 8 ) == 0 );    //!< Compile-time assert on size of structure

// The NBFDirectedLight_nbf_53 structure represents a directed light source.
/** The object code for a NBFDirectedLight_nbf_53 is DPBFCode::DIRECTED_LIGHT. */
struct NBFDirectedLight_nbf_53 : public NBFLightSource_nbf_53
{
  float3_t direction; //!< Specifies the direction of the light source.
  PADDING(4);         //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
};
DP_STATIC_ASSERT( ( sizeof(NBFDirectedLight_nbf_53) % 8 ) == 0 );    //!< Compile-time assert on size of structure

// The NBFPointLight_nbf_53 structure represents a point light source.
/** The object code for a NBFPointLight_nbf_53 is DPBFCode::POINT_LIGHT. */
struct NBFPointLight_nbf_53 : public NBFLightSource_nbf_53
{
  float3_t position;    //!< Specifies the position of the light source.
  float3_t attenuation; //!< Specifies the attenuation factors of the point light.
                        //!< The x-component of the vector specifies the constant term of the attenuation,
                        //!< the y-component of the vector specifies the linear term of the attenuation, and
                        //!< the z-component of the vector specifies the quadratic term of the attenuation.
};
DP_STATIC_ASSERT( ( sizeof(NBFPointLight_nbf_53) % 8 ) == 0 );   //!< Compile-time assert on size of structure

// The NBFSpotLight_nbf_53 structure represents a spot light.
/** The object code for a NBFSpotLight_nbf_53 is DPBFCode::SPOT_LIGHT. */
struct NBFSpotLight_nbf_53 : public NBFLightSource_nbf_53
{
  float3_t position;        //!< Specifies the position of the light source.
  float3_t direction;       //!< Specifies the direction of the light source.
  float3_t attenuation;     //!< Specifies the attenuation factors of the spot light.
                            //!< The x-component of the vector specifies the constant term of the attenuation,
                            //!< the y-component of the vector specifies the linear term of the attenuation, and
                            //!< the z-component of the vector specifies the quadratic term of the attenuation.
  float    cutoffAngle;     //!< Specifies the angle between the axis of the cone, the light is emitted to, and
                            //!< a ray along the edge of the cone.
  float    falloffExponent; //!< Controls the intensity distribution inside the cone, the light is mitted to.
  PADDING(4);               //!< Padding bits to ensure the size of NBFSpotLight is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFSpotLight_nbf_53) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFStateAttribute_nbf_54 structure represents a state attribute.
/** A NBFStateAttribute_nbf_54 serves a base class only. Concrete object codes valid for a NBFStateAttribute_nbf_54
  * are DPBFCode::BLEND_ATTRIBUTE, DPBFCode::CGFX, DPBFCode::RTFX, DPBFCode::MATERIAL, DPBFCode::FACE_ATTRIBUTE, and DPBFCode::TEXTURE_ATTRIBUTE. */
struct NBFStateAttribute_nbf_54 : public NBFObject
{
};
DP_STATIC_ASSERT( ( sizeof(NBFStateAttribute_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFMaterial_nbf_54 structure represents a material.
/** The object code for a NBFMaterial_nbf_54 is DPBFCode::MATERIAL. */
struct NBFMaterial_nbf_54 : public NBFStateAttribute_nbf_54
{
  float3_t    frontAmbientColor;      //!< Specifies the ambient part of the front material color.
  float3_t    frontDiffuseColor;      //!< Specifies the diffuse part of the front material color.
  float3_t    frontEmissiveColor;     //!< Specifies the emissive part of the front material color.
  float       frontIndexOfRefraction; //!< Specifies the index of refraction of the front material.
  float3_t    frontOpacityColor;      //!< Specifies the opacity per color channel of the front material.
  float3_t    frontReflectivityColor; //!< Specifies the reflectivity per color channel of the front material.
  float3_t    frontSpecularColor;     //!< Specifies the specular part of the front material color.
  float       frontSpecularExponent;  //!< Specifies the specular exponent of the front material color.
  float3_t    backAmbientColor;       //!< Specifies the ambient part of the back material color.
  float3_t    backDiffuseColor;       //!< Specifies the diffuse part of the back material color.
  float3_t    backEmissiveColor;      //!< Specifies the emissive part of the back material color.
  float       backIndexOfRefraction;  //!< Specifies the index of refraction of the back material.
  float3_t    backOpacityColor;      //!< Specifies the opacity per color channel of the back material.
  float3_t    backReflectivityColor; //!< Specifies the reflectivity per color channel of the back material.
  float3_t    backSpecularColor;      //!< Specifies the specular part of the back material color.
  float       backSpecularExponent;   //!< Specifies the specular exponent of the back material color.
};
DP_STATIC_ASSERT( ( sizeof(NBFMaterial_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The texBinding_t_nbf_54 structure specifies how a texture binding is stored in a .DPBF file.
/** Texture bindings, in this context, need to be considered in conjunction with
  * NBFTextureAttribute_nbf_54 and NBFTextureAttributeItem_nbf_54 objects. */
struct texBinding_t_nbf_54
{
  uint_t      texUnit;            //!< Specifies the texture unit where the actual NBFTextureAttributeItem_nbf_54 object is bound to.
  uint_t      texAttribItem;      //!< Specifies the file offset to the corresponding NBFTextureAttributeItem_nbf_54 object.
};
DP_STATIC_ASSERT( ( sizeof(texBinding_t_nbf_54) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFTextureAttributeItem_nbf_54 structure represents a single texture object.
/** The object code for a NBFTextureAttributeItem_nbf_54 is DPBFCode::TEXTURE_ATTRIBUTE_ITEM.
  * A NBFTextureAttributeItem_nbf_54 needs to be consider in conjunction with a NBFTextureAttribute_nbf_54,
  * which specifies the binding of texture objects. */
struct NBFTextureAttributeItem_nbf_54 : public NBFObject
{
  uint_t        texImg;             //!< Specifies the file offset to the texture image object
  float4_t      texEnvColor;        //!< Specifies the texture environment color.
  uint_t        texEnvMode;         //!< Specifies the texture environment mode for the actual texture object.
                                    //!< Valid modes are TextureEnvMode::REPLACE, TextureEnvMode::MODULATE, TextureEnvMode::DECAL, TextureEnvMode::BLEND, and TextureEnvMode::ADD.
  uint_t        texEnvScale;        //!< Specifies the texture environment scale used with rasterization
  uint_t        texWrapS;           //!< Specifies the wrap parameter for texture coordinate s.
  uint_t        texWrapT;           //!< Specifies the wrap parameter for texture coordinate t.
  uint_t        texWrapR;           //!< Specifies the wrap parameter for texture coordinate r.
  uint_t        minFilter;          //!< Specifies the filter used with minimizing.
                                    //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t        magFilter;          //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  float4_t      texBorderColor;     //!< Specifies the texture border RGBA color.
  trafo_t       trafo;              //!< Specifies the texture transformation
  uint_t        texGenMode[4];      //!< Specifies the texture coordinate generation modes
  float4_t      texGenPlane[2][4];  //!< Specifies the texture coordinate generation planes
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttributeItem_nbf_54) % 8 ) == 0 );   // Compile-time assert on size of structure

//! The NBFTextureAttribute_nbf_54 structure represents a texture attribute.
/** The object code for a NBFTextureAttribute_nbf_54 is DPBFCode::TEXTURE_ATTRIBUTE. */
struct NBFTextureAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  uint_t      numBindings;        //!< Specifies the number of contained texture bindings.
  uint_t      bindings;           //!< Specifies the file offset to the texture bindings. Texture bindings in
                                  //!< the NBF format are stored as texBinding_t objects.
};
DP_STATIC_ASSERT( ( sizeof(NBFTextureAttribute_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFAlphaTestAttribute_nbf_54 structure represents an alpha test state attribute in the NBF format.
/** The object code for a NBFAlphaTestAttribute_nbf_54 is DPBFCode::ALPHA_TEST_ATTRIBUTE. */
struct NBFAlphaTestAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     alphaFunction; //!< Specifies the alpha function
  ubyte_t     enabledMSAlpha; //!< Specifies whether enabled during msalpha
  PADDING(2); //!< Padding bits to ensure offset of threshold member is on a 4-byte boundary, regardless of packing
  float       threshold; //!< Specifies the threshold value
};
DP_STATIC_ASSERT( ( sizeof(NBFAlphaTestAttribute_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFBlendAttribute_nbf_54 structure represents a blend function.
/** The object code for a NBFBlendAttribute_nbf_54 is DPBFCode::BLEND_ATTRIBUTE. */
struct NBFBlendAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     sourceFunction;       //!< Specifies the source blending function
  ubyte_t     destinationFunction;  //!< Specifies the destination blending function
  PADDING(6);                       //!< Padding bits to ensure size of NBFBlendAttribute_nbf_54 is a multiple of 8
};
DP_STATIC_ASSERT( ( sizeof(NBFBlendAttribute_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFaceAttribute_nbf_54 structure represents a render mode.
/** The object code for a NBFFaceAttribute_nbf_54 is DPBFCode::FACE_ATTRIBUTE. */
struct NBFFaceAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     cullMode;             //!< Specifies the face culling mode.
  ubyte_t     polygonOffsetEnabled; //!< Specifies if polygon offset should be enabled for rendering.
  PADDING(2);                       //!< Padding bits to ensure offset of polygonOffsetFactor is on a 4-byte boundary, regardless of packing
  float       polygonOffsetFactor;  //!< Specifies the scale factor to be used to create a variable depth offset
                                    //!< for a polygon.
  float       polygonOffsetUnits;   //!< Specifies a unit that is multiplied by a render API implementation-specific
                                    //!< value to create a constant depth offset.
  ubyte_t     twoSidedLighting;     //!< Specifies if two-sided lighting should be enabled for rendering.
  ubyte_t     frontFaceMode;        //!< Specifies the face mode (points/lines/faces) for front faces.
  ubyte_t     backFaceMode;         //!< Specifies the face mode (points/lines/faces) for back faces.
  ubyte_t     faceWindingCCW;       //!< Specifies if the face winding is counter clock wise
};
DP_STATIC_ASSERT( ( sizeof(NBFFaceAttribute_nbf_54) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFLightingAttribute_nbf_54 structure represents a render mode.
/** The object code for a NBFLightingAttribute_nbf_54 is DPBFCode::LIGHTING_ATTRIBUTE. */
struct NBFLightingAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     enabled;    //!< Specifies if lighting is enabled or disabled.
  PADDING(3);             //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFLightingAttribute_nbf_54) % 4 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFLineAttribute_nbf_54 structure represents a render mode.
/** The object code for a NBFLineAttribute_nbf_54 is DPBFCode::LINE_ATTRIBUTE. */
struct NBFLineAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     antiAliasing;         //!< Specifies if the lines are anti aliased.
  PADDING(3);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
  uint_t      stippleFactor;        //!< Specifies the stipple factor
  uint_t      stipplePattern;       //!< Specifies the stipple pattern
  float       width;                //!< Specifies the line width
};
DP_STATIC_ASSERT( ( sizeof(NBFLineAttribute_nbf_54) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFPointAttribute_nbf_54 structure represents a render mode.
/** The object code for a NBFPointAttribute_nbf_54 is DPBFCode::POINT_ATTRIBUTE. */
struct NBFPointAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  ubyte_t     antiAliasing;         //!< Specifies if the points are anti aliased.
  PADDING(3);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
  float       size;                 //!< Specifies the point size
};
DP_STATIC_ASSERT( ( sizeof(NBFPointAttribute_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

/*! \brief The NBFUnlitColorAttribute_nbf_54 structure represents the color for unlit objects.
 *  \remarks The object code for a NBFUnlitColorAttribute_nbf_54 is DPBFCode::UNLIT_COLOR_ATTRIBUTE. */
struct NBFUnlitColorAttribute_nbf_54 : public NBFStateAttribute_nbf_54
{
  float4_t      color;              //!< Specifies the color for unlit objects
};
DP_STATIC_ASSERT( ( sizeof(NBFUnlitColorAttribute_nbf_54) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFAnimatedTransform_nbf_54 structure represents an animated transform group node.
/** The object code for a NBFAnimatedTransform_nbf_54 is DPBFCode::ANIMATED_TRANSFORM. */
struct NBFAnimatedTransform_nbf_54 : public NBFTransform
{
  uint_t      animation;          //!< Specifies the file offset to the NBFTrafoAnimation object
                                  //!< to be applied to the transform group node.
  PADDING(4);        //!< Padding bits to ensure the size of NBFAnimatedTransform_nbf_54 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedTransform_nbf_54) % 8 ) == 0 );    //!< Compile-time assert on size of structure

/*! \brief The NBFAnimatedVertexAttributeSet_nbf_54 structure represents a set of animated vertex attributes.
 *  \remarks A NBFAnimatedVertexAttributeSet_nbf_54 derives from NBFVertexAttributeSet, and holds up to 16
 *  animations, one for each of the up to 16 vertex attributes in a NBFVertexAttributeSet, and a
 *  field of flags to specify if a vertex attribute has to be normalized, for example after
 *  interpolating. */
struct NBFAnimatedVertexAttributeSet_nbf_54 : public NBFVertexAttributeSet
{
  uint_t      animations[16];     //!< Specifies the file offset to the animation objects, one
                                  //!< offset for each vertex attribute.
  uint_t      normalizeFlags;     //!< Flags that determine if a vertex attribute has to be
                                  //!< normalized after interpolation
  PADDING(4);                     //!< Padding bits to ensure the size of NBFAnimatedVertexAttributeSet_nbf_54 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFAnimatedVertexAttributeSet_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFFlipbookAnimation_nbf_54 structure represents an animated group node.
/** The object code for a NBFFlipbookAnimation_nbf_54 is DPBFCode::FLIPBOOK_ANIMATION. */
struct NBFFlipbookAnimation_nbf_54 : public NBFGroup
{
  uint_t      animation;          //!< Specifies the file offset to the NBFIndexAnimation object
                                  //!< to be applied to the transform group node.
  PADDING(4);        //!< Padding bits to ensure the size of NBFFlipbookAnimation_nbf_54 is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFFlipbookAnimation_nbf_54) % 8 ) == 0 );    //!< Compile-time assert on size of structure

//! The NBFSkinnedTriangles_nbf_54 structure represents skin animated triangles.
/** A NBFSkinnedTriangles_nbf_54 is a concrete topology class derived from NBFIndependentPrimitiveSet.
* The object code for a NBFSkinnedTriangles_nbf_54 is DPBFCode::SKINNED_TRIANGLES. */
struct NBFSkinnedTriangles_nbf_54 : public NBFIndependentPrimitiveSet
{
  uint_t      numSkins;           //!< Specifies the number of contained skins
  uint_t      skins;              //!< Specifies the file offset to the corresponding skin_t objects.
};
DP_STATIC_ASSERT( ( sizeof(NBFSkinnedTriangles_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFStateSet_nbf_54 structure represents a set of StateAttributes
/** The object code for a NBFStateSet_nbf_54 is DPBFCode::STATE_SET. */
struct NBFStateSet_nbf_54 : public NBFObject
{
  uint_t      numStateAttribs;    //!< Specifies the number of contained state attributes.
  uint_t      stateAttribs;       //!< Specifies the file offset to the offsets to the NBFStateAttribute objects
};
DP_STATIC_ASSERT( ( sizeof(NBFStateSet_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

//! An NBFRTFxBase_nbf_54 is a common base for both RTFx and RTFxSceneAttribute
struct NBFRTFxBase_nbf_54
{
  uint_t  numParameters;          //!< Specifies the number of parameters.
  uint_t  parameters;             //!< Specifies the file offset of the parameters.
  //  NOTE: no need to store the textures! They're resolved by the associated RTFxProgram!
};
DP_STATIC_ASSERT( ( sizeof(NBFRTFxBase_nbf_54) % 4 ) == 0 );   //!< Compile-time assert on size of structure

//! The NBFRTFx_nbf_54 structure represents an effect attribute.
/** The object code for a NBFRTFx_nbf_54 is DPBFCode::RTFX. */
struct NBFRTFx_nbf_54 : public NBFStateAttribute_nbf_54, public NBFRTFxBase_nbf_54
{
  uint_t  program;                //!< Specifies the file offset of the RTFxProgram.
  PADDING(4);                     //!< Padding bits to ensure the size of NBFRTFx is a multiple of 8, regardless of packing.
};
DP_STATIC_ASSERT( ( sizeof(NBFRTFx_nbf_54) % 8 ) == 0 );   //!< Compile-time assert on size of structure

struct NBFSampler_nbf_54 : public NBFObject
{
  uint_t      samplerState;         //!< Specifies the offset of the SamplerState
  uint_t      texture;              //!< Specifies the offset of the Texture
};
DP_STATIC_ASSERT( ( sizeof(NBFSampler_nbf_54) % 8 ) == 0 );

struct NBFSamplerState_nbf_54 : public NBFObject
{
  float4_t    borderColor;          //!< Speicifies the texture border RGBA color
  uint_t      magFilter;            //!< Specifies the filter used with magnifying.
                                    //!< Valid values are TextureMagFilterMode::NEAREST, and TextureMagFilterMode::LINEAR.
  uint_t      minFilter;            //!< Specifies the filter used with minimizing.
                                    //!< //!< Valid values are TextureMinFilterMode::NEAREST, TextureMinFilterMode::LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_LINEAR,
                                    //!< TextureMinFilterMode::NEAREST_MIPMAP_NEAREST, TextureMinFilterMode::NEAREST_MIPMAP_LINEAR, TextureMinFilterMode::LINEAR_MIPMAP_NEAREST.
  uint_t      texWrapS;             //!< Specifies the wrap parameter for texture coordinate s.
  uint_t      texWrapT;             //!< Specifies the wrap parameter for texture coordinate t.
  uint_t      texWrapR;             //!< Specifies the wrap parameter for texture coordinate r.
  uint_t      compareMode;          //!> Specifies the compare mode parameter for a texture. Valid values are TextureCompareMode::NONEa and TextureCompareMode::R_TO_TEXTURE.
};
DP_STATIC_ASSERT( ( sizeof(NBFSamplerState_nbf_54) % 8 ) == 0 );

//! The NBFEffectData_nbf_55 structure represents a set of ParameterGroupDatas
/** The object code for an NBFEffectData_nbf_55 is DPBFCode::PIPELINE_DATA. */
struct NBFEffectData_nbf_55 : public NBFObject
{
  str_t       effectSpecName;       //!< Specifies the name of the corresponding EffectSpec
  uint_t      parameterGroupData;   //!< Specifies the file offset to the offsets to the NBFParameterGroupData objects
  ubyte_t     transparent;          //!< Specifies if this EffectData is to be handled as transparent
  PADDING(3);        //!< Padding bits to ensure the offset of the next struct member is on a 4-byte boundary.
};
DP_STATIC_ASSERT( ( sizeof(NBFEffectData_nbf_55) % 8 ) == 0 );   //!< Compile-time assert on size of structure

/*! \brief The NBFPatchesBase structure is the base of all patches structures. */
struct NBFPatchesBase : public NBFPrimitive
{
  // nothing required here any more
};
DP_STATIC_ASSERT( ( sizeof(NBFPatchesBase) % 8 ) == 0 );        //!< Compile-time assert on size of structure

/*! \brief The NBFQuadPatches structure represents a general quad patches object.
 *  \remarks The object code for a NBFQuadPatches is DPBFCode::QUAD_PATCHES. */
struct NBFQuadPatches : public NBFPatchesBase
{
  uint_t  size;                     //!< Specifies the size of the quad patches. Each patch is specified by size^2 vertices
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches) % 8 ) == 0 );        //!< Compile-time assert on size of structure

/*! \brief The NBFQuadPatches4x4 structure represents a 4x4 quad patches object.
 *  \remarks The object code for a NBFQuadPatches4x4 is DPBFCode::QUAD_PATCHES_4X4. */
struct NBFQuadPatches4x4 : public NBFPatchesBase
{
};
DP_STATIC_ASSERT( ( sizeof(NBFQuadPatches4x4) % 8 ) == 0 );     // Compile-time assert on size of structure

/*! \brief The NBFRectPatches structure represents a general rectangular patches object.
 *  \remarks The object code for a NBFRectPatches is DPBFCode::RECT_PATCHES. */
struct NBFRectPatches : public NBFPatchesBase
{
  uint_t  width;                  //!< Specifies the width of the patches. Each patch is specified by with*height vertices.
  uint_t  height;                 //!< Specifies the height of the patches. Each patch is specified by width*height vertices.
};
DP_STATIC_ASSERT( ( sizeof(NBFRectPatches) % 8 ) == 0 );      //!< Compile-time assert on size of structure

/*! \brief The NBFRectPatches structure represents a general triangular patches object.
 *  \remarks The object code for a NBFRectPatches is DPBFCode::TRI_PATCHES. */
struct NBFTriPatches : public NBFPatchesBase
{
  uint_t  size;                     //!< Specifies the size of the patches. Each patch is specified by 1+2+...+size vertices
  PADDING(4);                       //!< Padding bits ensure offset of next elements is on a 4-byte boundary, regardless of packing
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches) % 8 ) == 0 );     //!< Compile-time assert on size of structure

/*! \brief The NBFTriPatches4 structure represents a 4-vertices-per-edge triangular patches object.
 *  \remarks The object code for a NBFTriPatches4 is DPBFCode::TRI_PATCHES_4. */
struct NBFTriPatches4 : public NBFPatchesBase
{
};
DP_STATIC_ASSERT( ( sizeof(NBFTriPatches4) % 8 ) == 0 );      // Compile-time assert on size of structure

#pragma pack(pop)

inline dp::DataType DPBFLoader::convertDataType( unsigned int dataType )
{
  if ( (m_nbfMajor < 0x54) || (m_nbfMajor == 0x54 && m_nbfMinor < 0x02) )
  {
    switch (dataType)
    {
    case 0: // was DP_BYTE
      return dp::DataType::INT_8;
    case 1:
      return dp::DataType::UNSIGNED_INT_8; // was DP_UNSIGNED_BYTE
    case 2:
      return dp::DataType::INT_16; // was DP_SHORT
    case 3:
      return dp::DataType::UNSIGNED_INT_16; // was DP_UNSIGNED_SHORT
    case 4:
      return dp::DataType::INT_32; // was DP_INT
    case 5:
      return dp::DataType::UNSIGNED_INT_32; // was DP_UNSIGNED_INT
    case 6:
      return dp::DataType::FLOAT_32; // was DP_FLOAT
    case 7:
      return dp::DataType::FLOAT_64; // was DP_DOUBLE
    case ~0u:
      return dp::DataType::UNKNOWN;
    }
    DP_ASSERT( !"unsupported datatype");
    return dp::DataType::UNKNOWN;
  }
  else
  {
    return static_cast<dp::DataType>(dataType);
  }
}

inline Vec2f& assign(Vec2f& lhs, const float2_t& rhs)
{
  lhs[0] = rhs[0];
  lhs[1] = rhs[1];
  return lhs;
}

inline Vec3f& assign(Vec3f& lhs, const float3_t& rhs)
{
  lhs[0] = rhs[0];
  lhs[1] = rhs[1];
  lhs[2] = rhs[2];
  return lhs;
}

// works for Vec4f and Quatf
template <typename Vec4Type>
inline Vec4Type& assign(Vec4Type& lhs, const float4_t& rhs)
{
  lhs[0] = rhs[0];
  lhs[1] = rhs[1];
  lhs[2] = rhs[2];
  lhs[3] = rhs[3];
  return lhs;
}

inline Vec2f convert(const float2_t& from)
{ // use return value optimization here
  return Vec2f(from[0], from[1]);
}

inline Vec3f convert(const float3_t& from)
{ // use return value optimization here
  return Vec3f(from[0], from[1], from[2]);
}

// works for Vec4f and Quatf
template <typename Vec4Type>
inline Vec4Type convert(const float4_t& from)
{ // use return value optimization here
  return Vec4Type(from[0], from[1], from[2], from[3]);
}

inline Mat44f convert( const float44_t & from )
{
  return( Mat44f( { from[0][0], from[0][1], from[0][2], from[0][3]
                  , from[1][0], from[1][1], from[1][2], from[1][3]
                  , from[2][0], from[2][1], from[2][2], from[2][3]
                  , from[3][0], from[3][1], from[3][2], from[3][3] } ) );
}

inline Trafo convert(const trafo_t_nbf_f& from)
{
  Trafo trafo;
  trafo.setOrientation(convert<Quatf>(from.orientation));
  trafo.setScaling(convert(from.scaling));
  trafo.setTranslation(convert(from.translation));
  trafo.setCenter(convert(from.center));
  return trafo;
}

inline Trafo convert(const trafo_t& from)
{
  Trafo trafo;
  trafo.setOrientation(convert<Quatf>(from.orientation));
  trafo.setScaling(convert(from.scaling));
  trafo.setScaleOrientation(convert<Quatf>(from.scaleOrientation));
  trafo.setTranslation(convert(from.translation));
  trafo.setCenter(convert(from.center));
  return trafo;
}

#if defined(_WIN32)
// dll entry point
BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID lpReserved)
{
  if (reason == DLL_PROCESS_ATTACH)
  {
    int i=0;
  }

  return TRUE;
}
#elif defined(LINUX)
void lib_init()
{
  int i=0;
}
#endif

#if defined(LINUX)
extern "C"
{
#endif

DPBFLOADER_API bool getPlugInterface(const UPIID& piid, dp::util::PlugInSharedPtr & pi)
{
  if (piid == PIID_NBF_SCENE_LOADER || piid == PIID_DPBF_SCENE_LOADER)
  {
    pi = DPBFLoader::create();
    return(!!pi);
  }
  return false;
}

DPBFLOADER_API void queryPlugInterfacePIIDs( std::vector<dp::util::UPIID> & piids )
{
  piids.clear();

  piids.push_back(PIID_NBF_SCENE_LOADER);
  piids.push_back(PIID_DPBF_SCENE_LOADER);
}

#if defined(LINUX)
}
#endif

DPBFLoaderSharedPtr DPBFLoader::create()
{
  return( std::shared_ptr<DPBFLoader>( new DPBFLoader() ) );
}

DPBFLoader::DPBFLoader()
{
}

DPBFLoader::~DPBFLoader()
{
}

inline string DPBFLoader::mapString(const str_t& str)
{
  if ( str.numChars )
  {
    return string(Offset_AutoPtr<const char>(m_fm, callback(), str.chars, str.numChars+1));
  }
  return string();
}

inline string DPBFLoader::mapString(const sstr_t& str)
{
  if ( str.numChars )
  {
    return string(Offset_AutoPtr<const char>(m_fm, callback(), str.chars, str.numChars+1));
  }
  return string();
}

inline void DPBFLoader::mapObject(uint_t offset, const ObjectSharedPtr & object)
{
  DP_ASSERT( m_offsetObjectMap.find(offset) == m_offsetObjectMap.end() );
  m_offsetObjectMap[offset] = object; // map even invalid objects!
}

inline void DPBFLoader::remapObject(uint_t offset, const ObjectSharedPtr & object )
{
  DP_ASSERT( m_offsetObjectMap.find(offset) != m_offsetObjectMap.end() );
  DP_ASSERT( m_offsetObjectMap[offset] != object );
  m_offsetObjectMap.erase( offset );
  mapObject( offset, object );
}

// SceneLoader API
SceneSharedPtr DPBFLoader::load(const string& filename, dp::util::FileFinder const& fileFinder, dp::sg::ui::ViewStateSharedPtr & viewState)
{
  DP_ASSERT( m_textureImages.empty() );

  if ( !dp::util::fileExists(filename) )
  {
    throw dp::FileNotFoundException( filename );
  }

  // set locale temporarily to standard "C" locale
  dp::util::Locale tl("C");

  SceneSharedPtr scene;

  // take a copy of the given search pathes, we might need them with looking up
  // adherent files like texture image file or effect sources
  m_fileFinder = fileFinder;
  m_fileFinder.addSearchPath( dp::util::getFilePath( filename ) );

  try
  {
    // the resulting file name should be valid if we get here
    DP_ASSERT(!filename.empty());
    // map the file into our address space
    m_fm = new ReadMapping( filename );
    if ( m_fm->isValid() )
    {
      {
        Offset_AutoPtr<NBFHeader> nbfHdr(m_fm, callback(), 0);
                                              //^ the NBF header always is at offset 0 for a valid NBF file!
        DP_ASSERT(nbfHdr);

        if (  nbfHdr->signature[0] == '#'
          && nbfHdr->signature[1] == 'N'
          && nbfHdr->signature[2] == 'B'
          && nbfHdr->signature[3] == 'F'
          )
        {
          char mv = nbfHdr->nbfMajorVersion;
          if (    ( nbfHdr->dpMajorVersion <= DP_VER_MAJOR )
              &&  (   ( mv != 0x42 ) && ( mv != 0x43 ) )              //  don't support some alpha releases
              &&  (   ( ( 0x0a <= mv ) && ( mv <= 0x13 ) )            //  supported from rel2.x
                  ||  ( ( 0x20 <= mv ) && ( mv <= 0x22 ) )            //  supported from rel3.x
                  ||  ( ( 0x30 <= mv ) && ( mv <= DPBF_VER_MAJOR ) ) ) //  supported from main
              &&  !( ( mv == DPBF_VER_MAJOR ) && ( DPBF_VER_MINOR < nbfHdr->nbfMinorVersion ) ) )
          {
            // private copy the nbf version for checking later on
            m_nbfMajor = nbfHdr->nbfMajorVersion;
            m_nbfMinor = nbfHdr->nbfMinorVersion;
            m_nbfBugfix = nbfHdr->nbfBugfixLevel;

            switch ( m_nbfMajor )
            {
              case 0x0a:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_f;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_b;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_a;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_b;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_10;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_e;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_f;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x0b:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_f;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData     = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_b;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_b;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_10;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_e;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_f;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x0c:            // fall thru
              case 0x0d:            // fall thru
              case 0x0e:            // fall thru
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_f;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_10;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_e;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_f;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x0f:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_f;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_10;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_f;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_f;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x10:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_11;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_10;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_12;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_11;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x11:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_11;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_11;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_11;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_11;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_11;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_12;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_11;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x12 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_12;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_12;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_12;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_12;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_12;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_12;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_12;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x13 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_12;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard_nbf_12;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup_nbf_12;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_12;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD_nbf_12;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_12;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform_nbf_12;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x20 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_20;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x21 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x22 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x30 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch_nbf_30;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x31:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_31;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x32:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_37;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x36:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_37;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_36;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x37:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_37;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x38:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_38;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x39:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_3a;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x3a :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_3a;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState_nbf_39;
                break;
              case 0x3b:            // fall thru
              case 0x3c:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x3d:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x3e :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_3e;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x3f :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_3f;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_41;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x40 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_40;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_41;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x41 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene_nbf_41;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                break;
              case 0x42 :
              case 0x43 :
              case 0x44 :
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_44;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches_nbf_47;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4_nbf_47;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches_nbf_47;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches_nbf_47;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4_nbf_47;
                break;
              case 0x45:
              case 0x46:
              case 0x47:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_4c;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState = &DPBFLoader::loadViewState_nbf_4c;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches_nbf_47;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4_nbf_47;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches_nbf_47;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches_nbf_47;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4_nbf_47;
                break;
              case 0x48:
              case 0x49:
              case 0x4A:
              case 0x4B:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_4c;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_4b;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState_nbf_4c;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches_nbf_4d;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4_nbf_4d;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches_nbf_4d;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches_nbf_4d;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4_nbf_4d;
                break;
              case 0x4C:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost_nbf_4b;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera_nbf_4c;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState_nbf_4c;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches_nbf_4d;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4_nbf_4d;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches_nbf_4d;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches_nbf_4d;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4_nbf_4d;
                break;
              case 0x4D:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive_nbf_4d;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches_nbf_4d;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4_nbf_4d;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches_nbf_4d;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches_nbf_4d;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4_nbf_4d;
                break;

              case 0x4E:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_4e;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x4F:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_51;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_4f;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x50:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_51;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_50;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_54;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x51:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode_nbf_51;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_52;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_54;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x52:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_52;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_54;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x53:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource_nbf_53;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_54;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet_nbf_54;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x54:
                m_pfnLoadTextureHost = &DPBFLoader::loadTextureHost;
                m_pfnLoadAnimatedTransform = &DPBFLoader::loadAnimatedTransform_nbf_54;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadFaceAttribute  = &DPBFLoader::loadFaceAttribute_nbf_54;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadMaterial       = &DPBFLoader::loadMaterial_nbf_54;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler_nbf_54;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadStateSet       = &DPBFLoader::loadStateSet_nbf_54;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTextureAttributeItem = &DPBFLoader::loadTextureAttributeItem_nbf_54;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case 0x55:
                m_pfnLoadTextureHost    = &DPBFLoader::loadTextureHost;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData_nbf_55;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;

              case DPBF_VER_MAJOR:   // fall thru
              default:
                m_pfnLoadTextureHost    = &DPBFLoader::loadTextureHost;
                m_pfnLoadBillboard      = &DPBFLoader::loadBillboard;
                m_pfnLoadCamera         = &DPBFLoader::loadCamera;
                m_pfnLoadPipelineData   = &DPBFLoader::loadPipelineData;
                m_pfnLoadGeoNode        = &DPBFLoader::loadGeoNode;
                m_pfnLoadGroup          = &DPBFLoader::loadGroup;
                m_pfnLoadLightSource    = &DPBFLoader::loadLightSource;
                m_pfnLoadLOD            = &DPBFLoader::loadLOD;
                m_pfnLoadPrimitive      = &DPBFLoader::loadPrimitive;
                m_pfnLoadSampler        = &DPBFLoader::loadSampler;
                m_pfnLoadScene          = &DPBFLoader::loadScene;
                m_pfnLoadSwitch         = &DPBFLoader::loadSwitch;
                m_pfnLoadTransform      = &DPBFLoader::loadTransform;
                m_pfnLoadVertexAttributeSet = &DPBFLoader::loadVertexAttributeSet;
                m_pfnLoadViewState      = &DPBFLoader::loadViewState;
                m_pfnLoadQuadPatches    = &DPBFLoader::loadQuadPatches;
                m_pfnLoadQuadPatches4x4 = &DPBFLoader::loadQuadPatches4x4;
                m_pfnLoadRectPatches    = &DPBFLoader::loadRectPatches;
                m_pfnLoadTriPatches     = &DPBFLoader::loadTriPatches;
                m_pfnLoadTriPatches4    = &DPBFLoader::loadTriPatches4;
                break;
            }

            DP_ASSERT(nbfHdr->scene); // should always be valid offset to the scene
            scene = (this->*m_pfnLoadScene)(nbfHdr->scene);
            if ( scene )
            {
              viewState.reset();
              if ( nbfHdr->viewState)
              {
                viewState = (this->*m_pfnLoadViewState)(nbfHdr->viewState);
              }
            }

            // some postprocessing of links needed for older file versions
            DP_ASSERT( ( m_nbfMajor < 0x51 ) || m_lightSourceToGroup.empty() );
            if ( m_nbfMajor < 0x51 )
            {
              for ( map<LightSourceSharedPtr,GroupSharedPtr>::const_iterator it = m_lightSourceToGroup.begin() ; it != m_lightSourceToGroup.end() ; ++it )
              {
                it->second->addChild( it->first );
              }
              m_lightSourceToGroup.clear();
            }
          }
          else
          {
            // incompatible NBF file
            // ... determine context
            if ( nbfHdr->dpMajorVersion > DP_VER_MAJOR )
            {
              // incompatible DPBF version detected
              uint_t expected = (DP_VER_MAJOR<<16) | DP_VER_MINOR;
              uint_t detected = (nbfHdr->dpMajorVersion<<16) | nbfHdr->dpMinorVersion;
              INVOKE_CALLBACK(onIncompatibleFile(filename, "DPBF", expected, detected));
            }
            else
            {
              // incompatible DPBF version detected
              uint_t expected = (DPBF_VER_MAJOR<<16) | DPBF_VER_MINOR;
              uint_t detected = (nbfHdr->nbfMajorVersion<<16) | nbfHdr->nbfMinorVersion;
              INVOKE_CALLBACK(onIncompatibleFile(filename, "DPBF", expected, detected));
            }
          }
        }
        else
        {
          // invalid NBF file
          INVOKE_CALLBACK(onInvalidFile(filename, "NBF"));
        }
      }
      delete m_fm;
    }
  }
  // catch all exception here to do cleanup
  catch ( ... )
  {
    // TODO it would be better to have a local object for the state and provide a weak-ptr to the loader during the call of this function.
    // If we want to add reentrace support the state-object should be passed by the traversers.
    m_offsetObjectMap.clear();
    m_sharedObjectsMap.clear();
    m_textureImages.clear();
    m_stateSetToPipeline.clear();
    m_materialToPipelineData.clear();
    m_pipelineData.reset();
    m_fileFinder.clear();

    // pass on caught exception to next handler
    throw;
  }

  m_offsetObjectMap.clear();
  m_sharedObjectsMap.clear();
  m_textureImages.clear();
  m_stateSetToPipeline.clear();
  m_materialToPipelineData.clear();
  DP_ASSERT( !m_pipelineData );
  m_fileFinder.clear();

  return scene;
}

ObjectSharedPtr DPBFLoader::loadCustomObject(DPBFCode objectCode, uint_t offset)
{
  return( ObjectSharedPtr() );
}

SceneSharedPtr DPBFLoader::loadScene(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    scene->setAmbientColor((const Vec3f&)convert(scenePtr->ambientColor));
    scene->setBackColor(convert<Vec4f>(scenePtr->backColor));

    string file;
    scene->setBackImage( (this->*m_pfnLoadTextureHost)( scenePtr->backImg, file ) );

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
    readObjectLinks( scene, scenePtr );
  }
  return scene;
}

SceneSharedPtr DPBFLoader::loadScene_nbf_41(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene_nbf_41> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    scene->setAmbientColor((const Vec3f&)convert(scenePtr->ambientColor));
    scene->setBackColor(convert<Vec4f>(scenePtr->backColor));

    string file;
    scene->setBackImage( (this->*m_pfnLoadTextureHost)( scenePtr->backImg, file ) );

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
    readObjectLinks( scene, scenePtr );
  }
  return scene;
}

SceneSharedPtr DPBFLoader::loadScene_nbf_3e(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene_nbf_3e> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    scene->setAmbientColor((const Vec3f&)convert(scenePtr->ambientColor));
    scene->setBackColor(convert<Vec4f>(scenePtr->backColor));

    string file;
    scene->setBackImage( (this->*m_pfnLoadTextureHost)( scenePtr->backImg, file ) );

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
  }
  return scene;
}

SceneSharedPtr DPBFLoader::loadScene_nbf_37(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene_nbf_37> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    scene->setAmbientColor((const Vec3f&)convert(scenePtr->ambientColor));
    scene->setBackColor(convert<Vec4f>(scenePtr->backColor));

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
  }
  return scene;
}

SceneSharedPtr DPBFLoader::loadScene_nbf_31(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene_nbf_31> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    scene->setAmbientColor((const Vec3f&)convert(scenePtr->ambientColor));
    scene->setBackColor(Vec4f(convert(scenePtr->backColor),1.0f));

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
  }
  return scene;
}

SceneSharedPtr DPBFLoader::loadScene_nbf_b(uint_t offset)
{
  SceneSharedPtr scene = Scene::create();
  Offset_AutoPtr<NBFScene_nbf_b> scenePtr(m_fm, callback(), offset);
  {
    // scene specific data
    // no ambient color here !!
    scene->setBackColor(Vec4f(convert(scenePtr->backColor),1.0f));

    readSceneCameras(scene, scenePtr);
    readSceneRootNode(scene, scenePtr);
  }
  return scene;
}

template <typename NBFSceneType>
void DPBFLoader::readSceneCameras( SceneSharedPtr const& scene, const Offset_AutoPtr<NBFSceneType>& nbfScene )
{
  if ( nbfScene->numCameras )
  {
    // map camera offsets
    // note: cameras is an offset to offsets
    DP_ASSERT(nbfScene->cameras);
    Offset_AutoPtr<uint_t> camOffs(m_fm, callback(), nbfScene->cameras, nbfScene->numCameras);
    // load cameras from file and add it to the scene
    for ( unsigned int i=0; i<nbfScene->numCameras; ++i )
    {
      CameraSharedPtr camera((this->*m_pfnLoadCamera)(camOffs[i]));
      if ( camera )
      {
        scene->addCamera(camera);
      }
    }
  }
}

template <typename NBFSceneType>
void DPBFLoader::readObjectLinks( SceneSharedPtr const& scene, const Offset_AutoPtr<NBFSceneType> & nbfScene )
{
  // handle links between objects
  if ( nbfScene->numObjectLinks )
  {
    DP_ASSERT(!"callbacks not supported");
  }
}

template <typename NBFSceneType>
void DPBFLoader::readSceneRootNode( SceneSharedPtr const& scene, const Offset_AutoPtr<NBFSceneType>& nbfScene )
{
  if ( nbfScene->root )
  {
    NodeSharedPtr node(loadNode(nbfScene->root));
    if ( node )
    {
      scene->setRootNode(node);
    }
  }
}

dp::sg::ui::ViewStateSharedPtr DPBFLoader::loadViewState(uint_t offset)
{
  dp::sg::ui::ViewStateSharedPtr viewState = dp::sg::ui::ViewState::create();
  Offset_AutoPtr<NBFViewState> vwstatePtr(m_fm, callback(), offset);

  if ( vwstatePtr->camera )
  {
    CameraSharedPtr camera((this->*m_pfnLoadCamera)(vwstatePtr->camera));
    if ( camera )
    {
      viewState->setCamera(camera);
    }
  }

  // ... stereo
  viewState->setStereoAutomaticEyeDistanceAdjustment(!!vwstatePtr->isStereoAutomatic);
  viewState->setAutoClipPlanes(!!vwstatePtr->isAutoClipPlanes);
  viewState->setStereoAutomaticEyeDistanceFactor(vwstatePtr->stereoAutomaticFactor);
  viewState->setStereoEyeDistance(vwstatePtr->stereoEyeDistance);
  viewState->setTargetDistance(vwstatePtr->targetDistance);
  return viewState;
}

dp::sg::ui::ViewStateSharedPtr DPBFLoader::loadViewState_nbf_4c(uint_t offset)
{
  dp::sg::ui::ViewStateSharedPtr viewState = dp::sg::ui::ViewState::create();
  Offset_AutoPtr<NBFViewState> vwstatePtr(m_fm, callback(), offset);

  if ( vwstatePtr->camera )
  {
    CameraSharedPtr camera((this->*m_pfnLoadCamera)(vwstatePtr->camera));
    if ( camera )
    {
      viewState->setCamera(camera);
    }
  }

  // ... stereo
  viewState->setStereoAutomaticEyeDistanceAdjustment(!!vwstatePtr->isStereoAutomatic);
  viewState->setStereoAutomaticEyeDistanceFactor(vwstatePtr->stereoAutomaticFactor);
  viewState->setStereoEyeDistance(vwstatePtr->stereoEyeDistance);
  viewState->setTargetDistance(vwstatePtr->targetDistance);

  viewState->setAutoClipPlanes( m_autoClipPlanes_nbf_4c );
  return viewState;
}

dp::sg::ui::ViewStateSharedPtr DPBFLoader::loadViewState_nbf_39(uint_t offset)
{
  dp::sg::ui::ViewStateSharedPtr viewState = dp::sg::ui::ViewState::create();
  Offset_AutoPtr<NBFViewState_nbf_39> vwstatePtr(m_fm, callback(), offset);

  if ( vwstatePtr->camera )
  {
    CameraSharedPtr camera((this->*m_pfnLoadCamera)(vwstatePtr->camera));
    if ( camera )
    {
      viewState->setCamera(camera);
    }
  }
  // ... stereo
  viewState->setStereoAutomaticEyeDistanceAdjustment(!!vwstatePtr->isStereoAutomatic);
  viewState->setStereoAutomaticEyeDistanceFactor(vwstatePtr->stereoAutomaticFactor);
  viewState->setStereoEyeDistance(vwstatePtr->stereoEyeDistance);
  viewState->setTargetDistance(vwstatePtr->targetDistance);

  viewState->setAutoClipPlanes(m_autoClipPlanes_nbf_4c);

  return viewState;
}

CameraSharedPtr DPBFLoader::loadCamera(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Camera * cam = NULL;
    Offset_AutoPtr<NBFCamera> camPtr(m_fm, callback(), offset);

    // camera could be either a parallel camera or a perspective camera
    DP_ASSERT(   camPtr->objectCode==DPBFCode::MATRIX_CAMERA
              || camPtr->objectCode==DPBFCode::PARALLEL_CAMERA
              || camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA
              || camPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( camPtr->objectCode )
    {
      case DPBFCode::MATRIX_CAMERA      : DP_VERIFY(loadMatrixCamera(offset));      break;
      case DPBFCode::PARALLEL_CAMERA    : DP_VERIFY(loadParallelCamera(offset));    break;
      case DPBFCode::PERSPECTIVE_CAMERA : DP_VERIFY(loadPerspectiveCamera(offset)); break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(camPtr->objectCode, offset));
    }
    // camera should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<Camera>(m_offsetObjectMap[offset]);
}

CameraSharedPtr DPBFLoader::loadCamera_nbf_4c(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Camera * cam = NULL;
    Offset_AutoPtr<NBFCamera> camPtr(m_fm, callback(), offset);

    // camera could be either a parallel camera or a perspective camera
    DP_ASSERT(   camPtr->objectCode==DPBFCode::MATRIX_CAMERA
                || camPtr->objectCode==DPBFCode::PARALLEL_CAMERA
                || camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA
                || camPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( camPtr->objectCode )
    {
      case DPBFCode::MATRIX_CAMERA      : DP_VERIFY(loadMatrixCamera(offset));              break;
      case DPBFCode::PARALLEL_CAMERA    : DP_VERIFY(loadParallelCamera_nbf_4c(offset));     break;
      case DPBFCode::PERSPECTIVE_CAMERA : DP_VERIFY(loadPerspectiveCamera_nbf_4c(offset));  break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(camPtr->objectCode, offset));
    }
    // camera should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<Camera>(m_offsetObjectMap[offset]);
}

CameraSharedPtr DPBFLoader::loadCamera_nbf_44(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Camera * cam = NULL;
    Offset_AutoPtr<NBFCamera_nbf_44> camPtr(m_fm, callback(), offset);

    // camera could be either a parallel camera or a perspective camera
    DP_ASSERT(  camPtr->objectCode==DPBFCode::PARALLEL_CAMERA
               || camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA
               || camPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( camPtr->objectCode )
    {
      case DPBFCode::PARALLEL_CAMERA :    DP_VERIFY(loadParallelCamera_nbf_44(offset));    break;
      case DPBFCode::PERSPECTIVE_CAMERA : DP_VERIFY(loadPerspectiveCamera_nbf_44(offset)); break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(camPtr->objectCode, offset));
    }
    // camera should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<Camera>(m_offsetObjectMap[offset]);
}

MatrixCameraSharedPtr DPBFLoader::loadMatrixCamera(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFMatrixCamera> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::MATRIX_CAMERA);

  MatrixCameraSharedPtr camera = MatrixCamera::create();
  readCamera( camera, camPtr );
  camera->setMatrices( convert( camPtr->projection ), convert( camPtr->inverseProjection ) );

  mapObject(offset, camera);
  return camera;
}

ParallelCameraSharedPtr DPBFLoader::loadParallelCamera(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFParallelCamera> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PARALLEL_CAMERA);

  ParallelCameraSharedPtr camera = ParallelCamera::create();
  readFrustumCamera( camera, offset );
  // a parallel camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

ParallelCameraSharedPtr DPBFLoader::loadParallelCamera_nbf_4c(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFParallelCamera_nbf_4c> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PARALLEL_CAMERA);

  ParallelCameraSharedPtr camera = ParallelCamera::create();
  readFrustumCamera_nbf_4c( camera, offset );
  // a parallel camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

ParallelCameraSharedPtr DPBFLoader::loadParallelCamera_nbf_44(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFParallelCamera_nbf_44> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PARALLEL_CAMERA);

  ParallelCameraSharedPtr camera = ParallelCamera::create();
  readCamera_nbf_44( camera, camPtr );
  // a parallel camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

PerspectiveCameraSharedPtr DPBFLoader::loadPerspectiveCamera(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPerspectiveCamera> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA);

  PerspectiveCameraSharedPtr camera = PerspectiveCamera::create();
  readFrustumCamera( camera, offset );
  // a perspective camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

PerspectiveCameraSharedPtr DPBFLoader::loadPerspectiveCamera_nbf_4c(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPerspectiveCamera_nbf_4c> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA);

  PerspectiveCameraSharedPtr camera = PerspectiveCamera::create();
  readFrustumCamera_nbf_4c( camera, offset );
  // a perspective camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

PerspectiveCameraSharedPtr DPBFLoader::loadPerspectiveCamera_nbf_44(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPerspectiveCamera_nbf_44> camPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(camPtr->objectCode==DPBFCode::PERSPECTIVE_CAMERA);

  PerspectiveCameraSharedPtr camera = PerspectiveCamera::create();
  readCamera_nbf_44( camera, camPtr );
  // a perspective camera does not have additional data

  mapObject(offset, camera);
  return camera;
}

PrimitiveSharedPtr DPBFLoader::loadAnyPrimitive(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFPrimitive> drawablePtr(m_fm, callback(), offset);

    DP_ASSERT(   drawablePtr->objectCode==DPBFCode::TRIANGLES
              || drawablePtr->objectCode==DPBFCode::ANIMATED_TRIANGLES
              || drawablePtr->objectCode==DPBFCode::SKINNED_TRIANGLES
              || drawablePtr->objectCode==DPBFCode::TRISTRIPS
              || drawablePtr->objectCode==DPBFCode::QUADMESHES
              || drawablePtr->objectCode==DPBFCode::QUADS
              || drawablePtr->objectCode==DPBFCode::ANIMATED_QUADS
              || drawablePtr->objectCode==DPBFCode::QUADSTRIPS
              || drawablePtr->objectCode==DPBFCode::LINES
              || drawablePtr->objectCode==DPBFCode::LINESTRIPS
              || drawablePtr->objectCode==DPBFCode::TRIFANS
              || drawablePtr->objectCode==DPBFCode::POINTS
              || drawablePtr->objectCode==DPBFCode::PATCHES
              || drawablePtr->objectCode==DPBFCode::QUAD_PATCHES
              || drawablePtr->objectCode==DPBFCode::QUAD_PATCHES_4X4
              || drawablePtr->objectCode==DPBFCode::RECT_PATCHES
              || drawablePtr->objectCode==DPBFCode::TRI_PATCHES
              || drawablePtr->objectCode==DPBFCode::TRI_PATCHES_4
              || drawablePtr->objectCode==DPBFCode::PRIMITIVE
              || drawablePtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( drawablePtr->objectCode )
    {
#define MK_PLIST(T) T,T##Handle
      case DPBFCode::ANIMATED_QUADS:     DP_VERIFY(loadAnimatedIndependents_nbf_3a(offset)); break;
      case DPBFCode::ANIMATED_TRIANGLES: DP_VERIFY(loadAnimatedIndependents_nbf_3a(offset)); break;
      case DPBFCode::LINES:              DP_VERIFY(loadIndependents(offset));                break;
      case DPBFCode::LINESTRIPS:         DP_VERIFY(loadStrips(offset));                      break;
      case DPBFCode::QUADMESHES:         DP_VERIFY(loadMeshes(offset));                      break;
      case DPBFCode::QUADS:              DP_VERIFY(loadIndependents(offset));                break;
      case DPBFCode::QUADSTRIPS:         DP_VERIFY(loadStrips(offset));                      break;
      case DPBFCode::SKINNED_TRIANGLES:  DP_VERIFY(loadSkinnedTriangles(offset));            break;
      case DPBFCode::TRIANGLES:          DP_VERIFY(loadIndependents(offset));                break;
      case DPBFCode::TRISTRIPS:          DP_VERIFY(loadStrips(offset));                      break;
      case DPBFCode::TRIFANS:            DP_VERIFY(loadStrips(offset));                      break;
      case DPBFCode::POINTS:             DP_VERIFY(loadIndependents(offset));                break;
      case DPBFCode::PRIMITIVE:          DP_VERIFY((this->*m_pfnLoadPrimitive)(offset));     break;

      // patches is only in m_nbfMajor < 0x48
      case DPBFCode::PATCHES:            DP_VERIFY( loadPatches_nbf_47(offset) );                  break;

      case DPBFCode::QUAD_PATCHES:       DP_VERIFY( (this->*m_pfnLoadQuadPatches)(offset) );       break;
      case DPBFCode::QUAD_PATCHES_4X4:   DP_VERIFY( (this->*m_pfnLoadQuadPatches4x4)(offset) );    break;
      case DPBFCode::RECT_PATCHES:       DP_VERIFY( (this->*m_pfnLoadRectPatches)(offset) );       break;
      case DPBFCode::TRI_PATCHES:        DP_VERIFY( (this->*m_pfnLoadTriPatches)(offset) );        break;
      case DPBFCode::TRI_PATCHES_4:      DP_VERIFY( (this->*m_pfnLoadTriPatches4)(offset) );       break;

#undef MK_PLIST
      // custom object handling
      default: mapObject(offset, loadCustomObject(drawablePtr->objectCode, offset));
    }
    // primitive should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]);
}

// loader for old-fashioned SkinnedTriangles; are mapped to Skin in a Triangles
PrimitiveSharedPtr DPBFLoader::loadSkinnedTriangles(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  PrimitiveSharedPtr trianglesHdl;
  Offset_AutoPtr<NBFSkinnedTriangles_nbf_54> triPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(triPtr->objectCode==DPBFCode::SKINNED_TRIANGLES);

  // need to care about object sharing
  if ( !loadSharedObject<Primitive>(trianglesHdl, triPtr, PrimitiveType::TRIANGLES) )
  {
    // NOTE: general object data were already written by loadSharedObject
    DP_ASSERT( trianglesHdl->getPrimitiveType() == PrimitiveType::TRIANGLES );
    readIndependentPrimitiveSet( trianglesHdl, triPtr );
  }
  mapObject(offset, trianglesHdl);
  return trianglesHdl;
}

PrimitiveSharedPtr DPBFLoader::loadAnimatedIndependents_nbf_3a( uint_t offset )
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  PrimitiveSharedPtr animatedHdl;
  Offset_AutoPtr<NBFAnimatedIndependents_nbf_3a> animatedPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(    animatedPtr->objectCode==DPBFCode::ANIMATED_QUADS
            ||  animatedPtr->objectCode==DPBFCode::ANIMATED_TRIANGLES );

  // need to care about object sharing
  if ( !loadSharedObject<Primitive>( animatedHdl, animatedPtr, animatedPtr->objectCode == DPBFCode::ANIMATED_QUADS ? PrimitiveType::QUADS : PrimitiveType::TRIANGLES ) )
  {
    // NOTE: general object data were already written by loadSharedObject
    readIndependentPrimitiveSet( animatedHdl, animatedPtr );
    DP_ASSERT( animatedHdl->getVertexAttributeSet() );
  }
  mapObject(offset, animatedHdl);
  return( animatedHdl );
}

PrimitiveSharedPtr DPBFLoader::loadIndependents( uint_t offset )
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  PrimitiveSharedPtr independentHdl;
  Offset_AutoPtr<NBFIndependentPrimitiveSet> indPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(  indPtr->objectCode==DPBFCode::LINES
          ||  indPtr->objectCode==DPBFCode::QUADS
          ||  indPtr->objectCode==DPBFCode::POINTS
          ||  indPtr->objectCode==DPBFCode::TRIANGLES );

  PrimitiveType pt;
  switch( indPtr->objectCode )
  {
    case DPBFCode::LINES :
      pt = PrimitiveType::LINES;
      break;
    case DPBFCode::QUADS :
      pt = PrimitiveType::QUADS;
      break;
    case DPBFCode::POINTS :
      pt = PrimitiveType::POINTS;
      break;
    case DPBFCode::TRIANGLES :
      pt = PrimitiveType::TRIANGLES;
      break;
    default :
      DP_ASSERT( false );
      break;
  }

  // need to care about object sharing
  if ( !loadSharedObject<Primitive>( independentHdl, indPtr, pt ) )
  {
    // NOTE: general object data were already written by loadSharedObject
    readIndependentPrimitiveSet( independentHdl, indPtr );
  }
  mapObject( offset, independentHdl );
  return( independentHdl );
}

PrimitiveSharedPtr DPBFLoader::loadMeshes(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  PrimitiveSharedPtr meshesHdl;
  Offset_AutoPtr<NBFMeshedPrimitiveSet> meshesPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT( meshesPtr->objectCode==DPBFCode::QUADMESHES );

  if ( !loadSharedObject<Primitive>( meshesHdl, meshesPtr, PrimitiveType::QUADS ) )
  {
    // NOTE: general object data have already been written by loadSharedObject
    readPrimitiveSet( meshesHdl, meshesPtr);

    // meshes
    DP_ASSERT(meshesPtr->numMeshes);
    Offset_AutoPtr<meshSet_t> mSets(m_fm, callback(), meshesPtr->meshes, meshesPtr->numMeshes);
    vector<unsigned int> meshSet;
    for ( unsigned int i=0; i<meshesPtr->numMeshes; ++i )
    {
      Offset_AutoPtr<uint_t> indices(m_fm, callback(), mSets[i].indices, mSets[i].width*mSets[i].height);
      for ( unsigned int row = 0 ; row < mSets[i].height-1 ; row++ )
      {
        for ( unsigned int col = 0 ; col < mSets[i].width-1 ; col++ )
        {
          meshSet.push_back( indices[ row     * mSets[i].width + col   ] );
          meshSet.push_back( indices[ row     * mSets[i].width + col+1 ] );
          meshSet.push_back( indices[ (row+1) * mSets[i].width + col+1 ] );
          meshSet.push_back( indices[ (row+1) * mSets[i].width + col   ] );
        }
      }
    }
    IndexSetSharedPtr iset( IndexSet::create() );
    iset->setData( &meshSet[0], dp::checked_cast<unsigned int>(meshSet.size()) );

    meshesHdl->setIndexSet( iset );
  }
  mapObject(offset, meshesHdl);
  return meshesHdl;
}

PrimitiveSharedPtr DPBFLoader::loadStrips(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  PrimitiveSharedPtr stripsHdl;
  Offset_AutoPtr<NBFStrippedPrimitiveSet> stripsPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(  stripsPtr->objectCode==DPBFCode::TRISTRIPS
          ||  stripsPtr->objectCode==DPBFCode::TRIFANS
          ||  stripsPtr->objectCode==DPBFCode::QUADSTRIPS
          ||  stripsPtr->objectCode==DPBFCode::LINESTRIPS );

  PrimitiveType pt;
  switch( stripsPtr->objectCode )
  {
    case DPBFCode::TRISTRIPS :
      pt = PrimitiveType::TRIANGLE_STRIP;
      break;
    case DPBFCode::TRIFANS :
      pt = PrimitiveType::TRIANGLE_FAN;
      break;
    case DPBFCode::QUADSTRIPS :
      pt = PrimitiveType::QUAD_STRIP;
      break;
    case DPBFCode::LINESTRIPS :
      pt = PrimitiveType::LINE_STRIP;
      break;
    default :
      DP_ASSERT( false );
      break;
  }

  if ( !loadSharedObject<Primitive>( stripsHdl, stripsPtr, pt ) )
  {
    // NOTE: general object data have already been written by loadSharedObject
    readPrimitiveSet( stripsHdl, stripsPtr );

    // strips
    DP_ASSERT(stripsPtr->numStrips);
    Offset_AutoPtr<indexList_t> iSets(m_fm, callback(), stripsPtr->strips, stripsPtr->numStrips);
    vector<unsigned int> stripSet;
    for ( unsigned int i=0; i<stripsPtr->numStrips; ++i )
    {
      Offset_AutoPtr<uint_t> indices(m_fm, callback(), iSets[i].indices, iSets[i].numIndices);
      stripSet.insert( stripSet.end(), &indices[0], &indices[iSets[i].numIndices] );
      stripSet.push_back( ~0 );
    }
    stripSet.pop_back();
    IndexSetSharedPtr iset( IndexSet::create() );
    iset->setData( &stripSet[0], dp::checked_cast<unsigned int>(stripSet.size()) );

    stripsHdl->setIndexSet( iset );
  }
  mapObject(offset, stripsHdl);
  return stripsHdl;
}

NodeSharedPtr DPBFLoader::loadNode(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFNode> nodePtr(m_fm, callback(), offset);

    DP_ASSERT(   nodePtr->objectCode==DPBFCode::GEO_NODE
              || nodePtr->objectCode==DPBFCode::GROUP
              || nodePtr->objectCode==DPBFCode::BILLBOARD
              || nodePtr->objectCode==DPBFCode::FLIPBOOK_ANIMATION
              || nodePtr->objectCode==DPBFCode::LOD
              || nodePtr->objectCode==DPBFCode::SWITCH
              || nodePtr->objectCode==DPBFCode::TRANSFORM
              || nodePtr->objectCode==DPBFCode::ANIMATED_TRANSFORM
              || nodePtr->objectCode==DPBFCode::VOLUME_NODE
              || nodePtr->objectCode==DPBFCode::DIRECTED_LIGHT
              || nodePtr->objectCode==DPBFCode::POINT_LIGHT
              || nodePtr->objectCode==DPBFCode::SPOT_LIGHT
              || nodePtr->objectCode==DPBFCode::LIGHT_SOURCE
              || nodePtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    // switch to concrete objects
    switch( nodePtr->objectCode )
    {
      case DPBFCode::GEO_NODE:           DP_VERIFY((this->*m_pfnLoadGeoNode)(offset));            break;
      case DPBFCode::GROUP:              DP_VERIFY((this->*m_pfnLoadGroup)(offset));              break;
      case DPBFCode::BILLBOARD:          DP_VERIFY((this->*m_pfnLoadBillboard)(offset));          break;
      case DPBFCode::FLIPBOOK_ANIMATION: DP_VERIFY(loadFlipbookAnimation(offset));                break;
      case DPBFCode::LOD:                DP_VERIFY((this->*m_pfnLoadLOD)(offset));                break;
      case DPBFCode::SWITCH:             DP_VERIFY((this->*m_pfnLoadSwitch)(offset));             break;
      case DPBFCode::TRANSFORM:          DP_VERIFY((this->*m_pfnLoadTransform)(offset));          break;
      case DPBFCode::ANIMATED_TRANSFORM: DP_VERIFY((this->*m_pfnLoadAnimatedTransform)(offset));  break;
      case DPBFCode::LIGHT_SOURCE:       // fall thru
      case DPBFCode::DIRECTED_LIGHT:     // fall thru
      case DPBFCode::POINT_LIGHT:        // fall thru
      case DPBFCode::SPOT_LIGHT:         DP_VERIFY((this->*m_pfnLoadLightSource)(offset));        break;
      case DPBFCode::VOLUME_NODE:        mapObject( offset, ObjectSharedPtr() );                  break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(nodePtr->objectCode, offset));
    }
    // node should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<Node>(m_offsetObjectMap[offset]);
}

ObjectSharedPtr DPBFLoader::loadNode_nbf_12(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFNode> nodePtr(m_fm, callback(), offset);

    DP_ASSERT(   nodePtr->objectCode==DPBFCode::GEO_NODE
              || nodePtr->objectCode==DPBFCode::GROUP
              || nodePtr->objectCode==DPBFCode::BILLBOARD
              || nodePtr->objectCode==DPBFCode::LOD
              || nodePtr->objectCode==DPBFCode::SWITCH
              || nodePtr->objectCode==DPBFCode::TRANSFORM
              || nodePtr->objectCode==DPBFCode::ANIMATED_TRANSFORM
              || nodePtr->objectCode==DPBFCode::DIRECTED_LIGHT
              || nodePtr->objectCode==DPBFCode::POINT_LIGHT
              || nodePtr->objectCode==DPBFCode::SPOT_LIGHT
              || nodePtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    // switch to concrete objects
    switch( nodePtr->objectCode )
    {
    case DPBFCode::GEO_NODE:           DP_VERIFY((this->*m_pfnLoadGeoNode)(offset));            break;
    case DPBFCode::GROUP:              DP_VERIFY((this->*m_pfnLoadGroup)(offset));              break;
    case DPBFCode::BILLBOARD:          DP_VERIFY((this->*m_pfnLoadBillboard)(offset));          break;
    case DPBFCode::LOD:                DP_VERIFY((this->*m_pfnLoadLOD)(offset));                break;
    case DPBFCode::SWITCH:             DP_VERIFY((this->*m_pfnLoadSwitch)(offset));             break;
    case DPBFCode::TRANSFORM:          DP_VERIFY((this->*m_pfnLoadTransform)(offset));          break;
    case DPBFCode::ANIMATED_TRANSFORM: DP_VERIFY((this->*m_pfnLoadAnimatedTransform)(offset));  break;
    case DPBFCode::DIRECTED_LIGHT:     // fall thru
    case DPBFCode::POINT_LIGHT:        // fall thru
    case DPBFCode::SPOT_LIGHT:         DP_VERIFY((this->*m_pfnLoadLightSource)(offset));        break;
      // custom object handling
    default: mapObject(offset, loadCustomObject(nodePtr->objectCode, offset));
    }
    // node should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return m_offsetObjectMap[offset];
}

NodeSharedPtr DPBFLoader::loadGeoNode(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGeoNode> nodePtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(nodePtr->objectCode==DPBFCode::GEO_NODE);

  GeoNodeSharedPtr nodeHdl(GeoNode::create());
  readObject( nodeHdl, nodePtr);
  readNode( nodeHdl, nodePtr );

  if ( nodePtr->stateSet )
  {
    (this->*m_pfnLoadStateSet)(nodePtr->stateSet);
  }
  if ( nodePtr->materialPipeline )
  {
    nodeHdl->setMaterialPipeline( (this->*m_pfnLoadPipelineData)( nodePtr->materialPipeline ) );
  }
  if ( nodePtr->primitive )
  {
    nodeHdl->setPrimitive( loadAnyPrimitive( nodePtr->primitive ) );
  }
  mapObject(offset, nodeHdl);

  return nodeHdl;
}

NodeSharedPtr DPBFLoader::loadGeoNode_nbf_51(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGeoNode_nbf_51> nodePtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(nodePtr->objectCode==DPBFCode::GEO_NODE);

  GeoNodeSharedPtr nodeHdl(GeoNode::create());
  readObject( nodeHdl, nodePtr );
  readNode( nodeHdl, nodePtr );

  if ( nodePtr->stateSet )
  {
    (this->*m_pfnLoadStateSet)(nodePtr->stateSet);
    map<uint_t,dp::sg::core::PipelineDataSharedPtr>::const_iterator it = m_stateSetToPipeline.find( nodePtr->stateSet );
    if ( it != m_stateSetToPipeline.end() && it->second )
    {
      nodeHdl->setMaterialPipeline( it->second );
    }
  }
  if ( nodePtr->primitive )
  {
    nodeHdl->setPrimitive( loadAnyPrimitive( nodePtr->primitive ) );
  }
  mapObject(offset, nodeHdl);

  return nodeHdl;
}

NodeSharedPtr DPBFLoader::loadGeoNode_nbf_4e(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGeoNode_nbf_4e> nodePtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(nodePtr->objectCode==DPBFCode::GEO_NODE);

  GroupSharedPtr group = Group::create();
  readObject( group, nodePtr );
  readNode( group, nodePtr );

  if ( m_nbfMajor >= 0x0e )
  {
    Offset_AutoPtr<geometrySet_t> geoSets( m_fm, callback(), nodePtr->geometrySets, nodePtr->numStateSets );

    for ( unsigned int i=0; i<nodePtr->numStateSets; ++i )
    {
      map<uint_t,dp::sg::core::PipelineDataSharedPtr>::const_iterator it = m_stateSetToPipeline.end();
      if ( geoSets[i].stateSet )
      {
        (this->*m_pfnLoadStateSet)( geoSets[i].stateSet );
        it = m_stateSetToPipeline.find( geoSets[i].stateSet );
      }
      Offset_AutoPtr<uint_t> primitives(m_fm, callback(), geoSets[i].primitives, geoSets[i].numPrimitives);
      for ( unsigned int j=0; j<geoSets[i].numPrimitives; ++j )
      {
        GeoNodeSharedPtr geoNode = GeoNode::create();
        if ( geoSets[i].stateSet )
        {
          if ( it != m_stateSetToPipeline.end() && it->second )
          {
            geoNode->setMaterialPipeline( it->second );
          }
        }
        if ( primitives[j] )
        {
          geoNode->setPrimitive( loadAnyPrimitive( primitives[j] ) );
        }
        group->addChild( geoNode );
      }
    }
  }
  else
  {
    // handle major version 0x0d and below
    Offset_AutoPtr<geometrySet_t_nbf_d> geoSets(m_fm, callback(), nodePtr->geometrySets, nodePtr->numStateSets);

    for ( unsigned int i=0; i<nodePtr->numStateSets; ++i )
    {
      GeoNodeSharedPtr geoNode = GeoNode::create();
      if ( geoSets[i].stateSet )
      {
        (this->*m_pfnLoadStateSet)( geoSets[i].stateSet );
        map<uint_t,dp::sg::core::PipelineDataSharedPtr>::const_iterator it = m_stateSetToPipeline.find( geoSets[i].stateSet );
        if ( it != m_stateSetToPipeline.end() && it->second )
        {
          geoNode->setMaterialPipeline( it->second );
        }
      }
      if ( geoSets[i].primitive )
      {
        geoNode->setPrimitive( loadAnyPrimitive( geoSets[i].primitive ) );
      }
      group->addChild( geoNode );
    }
  }
  if ( group->getNumberOfChildren() == 1 )
  {
    NodeSharedPtr geoNode = *group->beginChildren();
    mapObject( offset, geoNode );
    return( geoNode );
  }
  else
  {
    mapObject( offset, group );
    return( group );
  }
}

GroupSharedPtr DPBFLoader::loadGroup(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGroup> groupPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(groupPtr->objectCode==DPBFCode::GROUP);

  GroupSharedPtr groupHdl(Group::create());
  readObject( groupHdl, groupPtr );
  readNode( groupHdl, groupPtr );
  readGroup( groupHdl, groupPtr );

  mapObject(offset, groupHdl);
  return groupHdl;
}

GroupSharedPtr DPBFLoader::loadGroup_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGroup_nbf_12> groupPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(groupPtr->objectCode==DPBFCode::GROUP);

  GroupSharedPtr groupHdl(Group::create());
  readObject( groupHdl, groupPtr );
  readNode( groupHdl, groupPtr );
  readGroup_nbf_12( groupHdl, groupPtr );

  mapObject(offset, groupHdl);
  return groupHdl;
}

GroupSharedPtr DPBFLoader::loadGroup_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFGroup_nbf_11> groupPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(groupPtr->objectCode==DPBFCode::GROUP);

  GroupSharedPtr groupHdl(Group::create());
  readObject( groupHdl, groupPtr );
  readNode( groupHdl, groupPtr );
  readGroup_nbf_11( groupHdl, groupPtr );

  mapObject(offset, groupHdl);
  return groupHdl;
}

BillboardSharedPtr DPBFLoader::loadBillboard(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFBillboard> billboardPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(billboardPtr->objectCode==DPBFCode::BILLBOARD);

  BillboardSharedPtr billboardHdl(Billboard::create());
  readObject( billboardHdl, billboardPtr );
  readNode( billboardHdl, billboardPtr );
  readGroup( billboardHdl, billboardPtr );

  // Billboard specific
  billboardHdl->setRotationAxis(convert(billboardPtr->rotationAxis));
  billboardHdl->setAlignment( (Billboard::Alignment)billboardPtr->alignment );

  mapObject(offset, billboardHdl);
  return billboardHdl;
}

BillboardSharedPtr DPBFLoader::loadBillboard_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFBillboard_nbf_12> billboardPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(billboardPtr->objectCode==DPBFCode::BILLBOARD);

  BillboardSharedPtr billboardHdl(Billboard::create());
  readObject( billboardHdl, billboardPtr );
  readNode( billboardHdl, billboardPtr );
  readGroup_nbf_12( billboardHdl, billboardPtr );

  // Billboard specific
  billboardHdl->setRotationAxis(convert(billboardPtr->rotationAxis));
  billboardHdl->setAlignment( billboardPtr->viewerAligned ? Billboard::Alignment::VIEWER : Billboard::Alignment::AXIS );

  mapObject(offset, billboardHdl);
  return billboardHdl;
}

BillboardSharedPtr DPBFLoader::loadBillboard_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFBillboard_nbf_11> billboardPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(billboardPtr->objectCode==DPBFCode::BILLBOARD);

  BillboardSharedPtr billboardHdl(Billboard::create());
  readObject( billboardHdl, billboardPtr );
  readNode( billboardHdl, billboardPtr );
  readGroup_nbf_11( billboardHdl, billboardPtr );

  // Billboard specific
  billboardHdl->setRotationAxis(convert(billboardPtr->rotationAxis));
  billboardHdl->setAlignment( billboardPtr->viewerAligned ? Billboard::Alignment::VIEWER : Billboard::Alignment::AXIS );

  mapObject(offset, billboardHdl);
  return billboardHdl;
}

SwitchSharedPtr DPBFLoader::loadFlipbookAnimation(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFFlipbookAnimation_nbf_54> animPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(animPtr->objectCode==DPBFCode::FLIPBOOK_ANIMATION);

  SwitchSharedPtr sharedSwitch(Switch::create());
  readObject( sharedSwitch, animPtr );
  readNode( sharedSwitch, animPtr );
  readGroup( sharedSwitch, animPtr );
  sharedSwitch->setActive( 0 );

  mapObject(offset, sharedSwitch);
  return sharedSwitch;
}

TransformSharedPtr DPBFLoader::loadTransform(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFTransform> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::TRANSFORM);

  TransformSharedPtr trafoHdl(Transform::create());
  readObject( trafoHdl, trafoPtr );
  readNode( trafoHdl, trafoPtr );
  readGroup( trafoHdl, trafoPtr );

  trafoHdl->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, trafoHdl);
  return trafoHdl;
}

TransformSharedPtr DPBFLoader::loadTransform_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFTransform_nbf_12> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::TRANSFORM);

  TransformSharedPtr trafoHdl(Transform::create());
  readObject( trafoHdl, trafoPtr );
  readNode( trafoHdl, trafoPtr );
  readGroup_nbf_12( trafoHdl, trafoPtr );

  trafoHdl->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, trafoHdl);
  return trafoHdl;
}

TransformSharedPtr DPBFLoader::loadTransform_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFTransform_nbf_11> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::TRANSFORM);

  TransformSharedPtr trafoHdl(Transform::create());
  readObject( trafoHdl, trafoPtr );
  readNode( trafoHdl, trafoPtr );
  readGroup_nbf_11( trafoHdl, trafoPtr );

  trafoHdl->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, trafoHdl);
  return trafoHdl;
}

TransformSharedPtr DPBFLoader::loadTransform_nbf_f(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFTransform_nbf_f> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::TRANSFORM);

  TransformSharedPtr trafoHdl(Transform::create());
  readObject( trafoHdl, trafoPtr );
  readNode( trafoHdl, trafoPtr );
  readGroup_nbf_11( trafoHdl, trafoPtr );

  trafoHdl->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, trafoHdl);
  return trafoHdl;
}

TransformSharedPtr DPBFLoader::loadAnimatedTransform_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFAnimatedTransform_nbf_54> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::ANIMATED_TRANSFORM);

  TransformSharedPtr transform = Transform::create();
  readObject( transform, trafoPtr );
  readNode( transform, trafoPtr );
  readGroup( transform, trafoPtr );
  transform->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, transform);
  return transform;
}

TransformSharedPtr DPBFLoader::loadAnimatedTransform_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFAnimatedTransform_nbf_12> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::ANIMATED_TRANSFORM);

  TransformSharedPtr transform = Transform::create();
  readObject( transform, trafoPtr );
  readNode( transform, trafoPtr );
  readGroup_nbf_12( transform, trafoPtr );
  transform->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, transform);
  return transform;
}

TransformSharedPtr DPBFLoader::loadAnimatedTransform_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFAnimatedTransform_nbf_11> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::ANIMATED_TRANSFORM);

  TransformSharedPtr transform = Transform::create();
  readObject( transform, trafoPtr );
  readNode( transform, trafoPtr );
  readGroup_nbf_11( transform, trafoPtr );
  transform->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, transform);
  return transform;
}

TransformSharedPtr DPBFLoader::loadAnimatedTransform_nbf_f(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFAnimatedTransform_nbf_f> trafoPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(trafoPtr->objectCode==DPBFCode::ANIMATED_TRANSFORM);

  TransformSharedPtr transform = Transform::create();
  readObject( transform, trafoPtr );
  readNode( transform, trafoPtr );
  readGroup_nbf_11( transform, trafoPtr );
  transform->setTrafo(convert(trafoPtr->trafo));

  mapObject(offset, transform);
  return transform;
}

LODSharedPtr DPBFLoader::loadLOD(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFLOD> lodPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lodPtr->objectCode==DPBFCode::LOD);

  LODSharedPtr lodHdl(LOD::create());
  readObject( lodHdl, lodPtr );
  readNode( lodHdl, lodPtr );
  readGroup( lodHdl, lodPtr );

  // LOD specific
  lodHdl->setCenter(convert(lodPtr->center));
  Offset_AutoPtr<float> ranges(m_fm, callback(), lodPtr->ranges, lodPtr->numRanges);
  lodHdl->setRanges(ranges, lodPtr->numRanges);

  mapObject(offset, lodHdl);
  return lodHdl;
}

LODSharedPtr DPBFLoader::loadLOD_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFLOD_nbf_12> lodPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lodPtr->objectCode==DPBFCode::LOD);

  LODSharedPtr lodHdl(LOD::create());
  readObject( lodHdl, lodPtr );
  readNode( lodHdl, lodPtr );
  readGroup_nbf_12( lodHdl, lodPtr );

  // LOD specific
  lodHdl->setCenter(convert(lodPtr->center));
  Offset_AutoPtr<float> ranges(m_fm, callback(), lodPtr->ranges, lodPtr->numRanges);
  lodHdl->setRanges(ranges, lodPtr->numRanges);

  mapObject(offset, lodHdl);
  return lodHdl;
}

LODSharedPtr DPBFLoader::loadLOD_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFLOD_nbf_11> lodPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lodPtr->objectCode==DPBFCode::LOD);

  LODSharedPtr lodHdl(LOD::create());
  readObject( lodHdl, lodPtr );
  readNode( lodHdl, lodPtr );
  readGroup_nbf_11( lodHdl, lodPtr );

  // LOD specific
  lodHdl->setCenter(convert(lodPtr->center));
  Offset_AutoPtr<float> ranges(m_fm, callback(), lodPtr->ranges, lodPtr->numRanges);
  lodHdl->setRanges(ranges, lodPtr->numRanges);

  mapObject(offset, lodHdl);
  return lodHdl;
}

SwitchSharedPtr DPBFLoader::loadSwitch(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSwitch> swtchPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(swtchPtr->objectCode==DPBFCode::SWITCH);

  SwitchSharedPtr swtchHdl(Switch::create());
  readObject( swtchHdl, swtchPtr );
  readNode( swtchHdl, swtchPtr );
  readGroup( swtchHdl, swtchPtr );

  // read-in Switch specific
  DP_ASSERT(swtchPtr->numMasks); // there should be at least a default mask
  Offset_AutoPtr<switchMask_t> masks(m_fm, callback(), swtchPtr->masks, swtchPtr->numMasks);
  for ( uint_t i=0; i<swtchPtr->numMasks; ++i )
  {
    Offset_AutoPtr<uint_t> children(m_fm, callback(), masks[i].children, masks[i].numChildren);
    Switch::SwitchMask mask(&children[0], &children[masks[i].numChildren]);
    swtchHdl->addMask(masks[i].maskKey, mask);
  }
  swtchHdl->setActiveMaskKey(swtchPtr->activeMaskKey);

  mapObject(offset, swtchHdl);
  return swtchHdl;
}

SwitchSharedPtr DPBFLoader::loadSwitch_nbf_30(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSwitch_nbf_30> swtchPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(swtchPtr->objectCode==DPBFCode::SWITCH);

  SwitchSharedPtr swtchHdl(Switch::create());
  readObject( swtchHdl, swtchPtr );
  readNode( swtchHdl, swtchPtr );
  readGroup( swtchHdl, swtchPtr );

  // Switch specific
  Offset_AutoPtr<uint_t> activeChilds(m_fm, callback(), swtchPtr->activeChildren, swtchPtr->numActiveChildren);
  for ( uint_t i=0; i<swtchPtr->numActiveChildren; ++i )
  { // note: Switch's internal index format is unsigned int
    swtchHdl->setActive(activeChilds[i]);
  }

  mapObject(offset, swtchHdl);
  return swtchHdl;
}

SwitchSharedPtr DPBFLoader::loadSwitch_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSwitch_nbf_12> swtchPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(swtchPtr->objectCode==DPBFCode::SWITCH);

  SwitchSharedPtr swtchHdl(Switch::create());
  readObject( swtchHdl, swtchPtr );
  readNode( swtchHdl, swtchPtr );
  readGroup_nbf_12( swtchHdl, swtchPtr );

  // Switch specific
  Offset_AutoPtr<uint_t> activeChilds(m_fm, callback(), swtchPtr->activeChildren, swtchPtr->numActiveChildren);
  for ( uint_t i=0; i<swtchPtr->numActiveChildren; ++i )
  { // note: Switch's internal index format is unsigned int
    swtchHdl->setActive(activeChilds[i]);
  }

  mapObject(offset, swtchHdl);
  return swtchHdl;
}

SwitchSharedPtr DPBFLoader::loadSwitch_nbf_11(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSwitch_nbf_11> swtchPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(swtchPtr->objectCode==DPBFCode::SWITCH);

  SwitchSharedPtr swtchHdl(Switch::create());
  readObject( swtchHdl, swtchPtr );
  readNode( swtchHdl, swtchPtr );
  readGroup_nbf_11( swtchHdl, swtchPtr );

  // Switch specific
  Offset_AutoPtr<uint_t> activeChilds(m_fm, callback(), swtchPtr->activeChildren, swtchPtr->numActiveChildren);
  for ( uint_t i=0; i<swtchPtr->numActiveChildren; ++i )
  { // note: Switch's internal index format is unsigned int
    swtchHdl->setActive(activeChilds[i]);
  }

  mapObject(offset, swtchHdl);
  return swtchHdl;
}

LightSourceSharedPtr DPBFLoader::loadLightSource(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFLightSource> lightPtr(m_fm, callback(), offset);

    DP_ASSERT(  lightPtr->objectCode==DPBFCode::LIGHT_SOURCE
             || lightPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    if ( lightPtr->objectCode == DPBFCode::LIGHT_SOURCE )
    {
      LightSourceSharedPtr lightSource = LightSource::create();
      readObject( lightSource, lightPtr );
      readNode( lightSource, lightPtr );
      lightSource->setShadowCasting( !!lightPtr->castShadow );
      lightSource->setEnabled( !!lightPtr->enabled );
      if ( lightPtr->lightEffect )
      {
        lightSource->setLightPipeline( (this->*m_pfnLoadPipelineData)( lightPtr->lightEffect ) );
      }

      mapObject( offset, lightSource );
    }
    else
    {
      mapObject(offset, loadCustomObject(lightPtr->objectCode, offset));
    }

    // light should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<LightSource>(m_offsetObjectMap[offset]);
}

LightSourceSharedPtr DPBFLoader::loadLightSource_nbf_53(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFLightSource_nbf_53> lightPtr(m_fm, callback(), offset);

    DP_ASSERT(  lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT
            ||  lightPtr->objectCode==DPBFCode::POINT_LIGHT
            ||  lightPtr->objectCode==DPBFCode::SPOT_LIGHT
            ||  lightPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( lightPtr->objectCode )
    {
      case DPBFCode::DIRECTED_LIGHT: DP_VERIFY(loadDirectedLight_nbf_53(offset)); break;
      case DPBFCode::POINT_LIGHT:    DP_VERIFY(loadPointLight_nbf_53(offset));    break;
      case DPBFCode::SPOT_LIGHT:     DP_VERIFY(loadSpotLight_nbf_53(offset));     break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(lightPtr->objectCode, offset));
    }
    // light should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<LightSource>(m_offsetObjectMap[offset]);
}

LightSourceSharedPtr DPBFLoader::loadLightSource_nbf_52(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFLightSource_nbf_52> lightPtr(m_fm, callback(), offset);

    DP_ASSERT(  lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT
            ||  lightPtr->objectCode==DPBFCode::POINT_LIGHT
            ||  lightPtr->objectCode==DPBFCode::SPOT_LIGHT
            ||  lightPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( lightPtr->objectCode )
    {
      case DPBFCode::DIRECTED_LIGHT: DP_VERIFY(loadDirectedLight_nbf_52(offset)); break;
      case DPBFCode::POINT_LIGHT:    DP_VERIFY(loadPointLight_nbf_52(offset));    break;
      case DPBFCode::SPOT_LIGHT:     DP_VERIFY(loadSpotLight_nbf_52(offset));     break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(lightPtr->objectCode, offset));
    }
    // light should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<LightSource>(m_offsetObjectMap[offset]);
}

LightSourceSharedPtr DPBFLoader::loadLightSource_nbf_50(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFLightSource_nbf_50> lightPtr(m_fm, callback(), offset);

    DP_ASSERT(  lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT
            ||  lightPtr->objectCode==DPBFCode::POINT_LIGHT
            ||  lightPtr->objectCode==DPBFCode::SPOT_LIGHT
            ||  lightPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( lightPtr->objectCode )
    {
      case DPBFCode::DIRECTED_LIGHT: DP_VERIFY(loadDirectedLight_nbf_50(offset)); break;
      case DPBFCode::POINT_LIGHT:    DP_VERIFY(loadPointLight_nbf_50(offset));    break;
      case DPBFCode::SPOT_LIGHT:     DP_VERIFY(loadSpotLight_nbf_50(offset));     break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(lightPtr->objectCode, offset));
    }
    // light should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<LightSource>(m_offsetObjectMap[offset]);
}

LightSourceSharedPtr DPBFLoader::loadLightSource_nbf_12(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFLightSource_nbf_12> lightPtr(m_fm, callback(), offset);

    DP_ASSERT(  lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT
            ||  lightPtr->objectCode==DPBFCode::POINT_LIGHT
            ||  lightPtr->objectCode==DPBFCode::SPOT_LIGHT
            ||  lightPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( lightPtr->objectCode )
    {
      case DPBFCode::DIRECTED_LIGHT: DP_VERIFY(loadDirectedLight_nbf_12(offset)); break;
      case DPBFCode::POINT_LIGHT:    DP_VERIFY(loadPointLight_nbf_12(offset));    break;
      case DPBFCode::SPOT_LIGHT:     DP_VERIFY(loadSpotLight_nbf_12(offset));     break;
        // custom object handling
      default: mapObject(offset, loadCustomObject(lightPtr->objectCode, offset));
    }
    // light should have been mapped if we get here
    DP_ASSERT(m_offsetObjectMap.find(offset)!=m_offsetObjectMap.end());
  }
  return std::static_pointer_cast<LightSource>(m_offsetObjectMap[offset]);
}

LightSourceSharedPtr DPBFLoader::loadDirectedLight_nbf_53(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFDirectedLight_nbf_53> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT);

  LightSourceSharedPtr directedLight = createStandardDirectedLight( convert( lightPtr->direction )
                                                                  , convert( lightPtr->ambientColor )
                                                                  , convert( lightPtr->diffuseColor )
                                                                  , convert( lightPtr->specularColor ) );
  readObject( directedLight, lightPtr );
  readNode( directedLight, lightPtr );
  readLightSource_nbf_53( directedLight, lightPtr );

  mapObject( offset, directedLight );
  return( directedLight );
}

LightSourceSharedPtr DPBFLoader::loadDirectedLight_nbf_52(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFDirectedLight_nbf_52> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT);

  LightSourceSharedPtr directedLight = createStandardDirectedLight( convert( lightPtr->direction )
                                                                  , convert( lightPtr->ambientColor )
                                                                  , convert( lightPtr->diffuseColor )
                                                                  , convert( lightPtr->specularColor ) );
  readObject( directedLight, lightPtr );
  readNode( directedLight, lightPtr );
  readLightSource_nbf_52( directedLight, lightPtr );

  mapObject( offset, directedLight );
  return( directedLight );
}

LightSourceSharedPtr DPBFLoader::loadDirectedLight_nbf_50(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFDirectedLight_nbf_50> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT);

  LightSourceSharedPtr directedLight = createStandardDirectedLight( convert( lightPtr->direction )
                                                                  , convert( lightPtr->ambientColor )
                                                                  , convert( lightPtr->diffuseColor )
                                                                  , convert( lightPtr->specularColor ) );
  readObject( directedLight, lightPtr );
  readLightSource_nbf_50( directedLight, lightPtr );

  mapObject( offset, directedLight );
  return( directedLight );
}

LightSourceSharedPtr DPBFLoader::loadDirectedLight_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFDirectedLight_nbf_12> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::DIRECTED_LIGHT);

  LightSourceSharedPtr directedLight = createStandardDirectedLight( convert( lightPtr->direction )
                                                                  , convert( lightPtr->ambientColor )
                                                                  , convert( lightPtr->diffuseColor )
                                                                  , convert( lightPtr->specularColor ) );
  readObject( directedLight, lightPtr );
  readNode( directedLight, lightPtr );
  readLightSource_nbf_12( directedLight, lightPtr );

  mapObject( offset, directedLight );
  return( directedLight );
}

LightSourceSharedPtr DPBFLoader::loadPointLight_nbf_53(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPointLight_nbf_53> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::POINT_LIGHT);

  LightSourceSharedPtr pointLight = createStandardPointLight( convert( lightPtr->position )
                                                            , convert( lightPtr->ambientColor )
                                                            , convert( lightPtr->diffuseColor )
                                                            , convert( lightPtr->specularColor )
                                                            , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] } );
  readObject( pointLight, lightPtr );
  readNode( pointLight, lightPtr );
  readLightSource_nbf_53( pointLight, lightPtr );

  mapObject( offset, pointLight );
  return( pointLight );
}

LightSourceSharedPtr DPBFLoader::loadPointLight_nbf_52(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPointLight_nbf_52> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::POINT_LIGHT);

  LightSourceSharedPtr pointLight = createStandardPointLight( convert( lightPtr->position )
                                                            , convert( lightPtr->ambientColor )
                                                            , convert( lightPtr->diffuseColor )
                                                            , convert( lightPtr->specularColor )
                                                            , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] } );
  readObject( pointLight, lightPtr );
  readNode( pointLight, lightPtr );
  readLightSource_nbf_52( pointLight, lightPtr );

  mapObject( offset, pointLight );
  return( pointLight );
}

LightSourceSharedPtr DPBFLoader::loadPointLight_nbf_50(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPointLight_nbf_50> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::POINT_LIGHT);

  LightSourceSharedPtr pointLight = createStandardPointLight( convert( lightPtr->position )
                                                            , convert( lightPtr->ambientColor )
                                                            , convert( lightPtr->diffuseColor )
                                                            , convert( lightPtr->specularColor )
                                                            , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] } );
  readObject( pointLight, lightPtr );
  readLightSource_nbf_50( pointLight, lightPtr );

  mapObject( offset, pointLight );
  return( pointLight );
}

LightSourceSharedPtr DPBFLoader::loadPointLight_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPointLight_nbf_12> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::POINT_LIGHT);

  LightSourceSharedPtr pointLight = createStandardPointLight( convert( lightPtr->position )
                                                            , convert( lightPtr->ambientColor )
                                                            , convert( lightPtr->diffuseColor )
                                                            , convert( lightPtr->specularColor )
                                                            , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] } );
  readObject( pointLight, lightPtr );
  readLightSource_nbf_12( pointLight, lightPtr );

  mapObject( offset, pointLight );
  return( pointLight );
}

LightSourceSharedPtr DPBFLoader::loadSpotLight_nbf_53(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSpotLight_nbf_53> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::SPOT_LIGHT);

  LightSourceSharedPtr spotLight = createStandardSpotLight( convert( lightPtr->position )
                                                          , convert( lightPtr->direction )
                                                          , lightPtr->intensity * convert( lightPtr->ambientColor )
                                                          , lightPtr->intensity * convert( lightPtr->diffuseColor )
                                                          , lightPtr->intensity * convert( lightPtr->specularColor )
                                                          , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] }
                                                          , lightPtr->falloffExponent
                                                          , radToDeg( lightPtr->cutoffAngle ) );
  readObject( spotLight, lightPtr );
  readNode( spotLight, lightPtr );
  readLightSource_nbf_53( spotLight, lightPtr );

  mapObject( offset, spotLight );
  return( spotLight );
}

LightSourceSharedPtr DPBFLoader::loadSpotLight_nbf_52(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSpotLight_nbf_52> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::SPOT_LIGHT);

  LightSourceSharedPtr spotLight = createStandardSpotLight( convert( lightPtr->position )
                                                          , convert( lightPtr->direction )
                                                          , lightPtr->intensity * convert( lightPtr->ambientColor )
                                                          , lightPtr->intensity * convert( lightPtr->diffuseColor )
                                                          , lightPtr->intensity * convert( lightPtr->specularColor )
                                                          , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] }
                                                          , lightPtr->falloffExponent
                                                          , radToDeg( lightPtr->cutoffAngle ) );
  readObject( spotLight, lightPtr );
  readNode( spotLight, lightPtr );
  readLightSource_nbf_52( spotLight, lightPtr );

  mapObject( offset, spotLight );
  return( spotLight );
}

LightSourceSharedPtr DPBFLoader::loadSpotLight_nbf_50(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSpotLight_nbf_50> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::SPOT_LIGHT);

  LightSourceSharedPtr spotLight = createStandardSpotLight( convert( lightPtr->position )
                                                          , convert( lightPtr->direction )
                                                          , lightPtr->intensity * convert( lightPtr->ambientColor )
                                                          , lightPtr->intensity * convert( lightPtr->diffuseColor )
                                                          , lightPtr->intensity * convert( lightPtr->specularColor )
                                                          , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] }
                                                          , lightPtr->falloffExponent
                                                          , radToDeg( lightPtr->cutoffAngle ) );
  readObject( spotLight, lightPtr );
  readLightSource_nbf_50( spotLight, lightPtr );

  mapObject( offset, spotLight );
  return( spotLight );
}

LightSourceSharedPtr DPBFLoader::loadSpotLight_nbf_12(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFSpotLight_nbf_12> lightPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(lightPtr->objectCode==DPBFCode::SPOT_LIGHT);

  // very old files had the angle as degree, was changed to rad later on
  float cutoffAngle = lightPtr->cutoffAngle;
  if ( ( 0x12 < m_nbfMajor ) || ( 0x12 == m_nbfMajor ) && ( 0 < m_nbfMinor ) )
  {
    cutoffAngle = radToDeg( cutoffAngle );
  }

  LightSourceSharedPtr spotLight = createStandardSpotLight( convert( lightPtr->position )
                                                          , convert( lightPtr->direction )
                                                          , lightPtr->intensity * convert( lightPtr->ambientColor )
                                                          , lightPtr->intensity * convert( lightPtr->diffuseColor )
                                                          , lightPtr->intensity * convert( lightPtr->specularColor )
                                                          , { lightPtr->attenuation[0], lightPtr->attenuation[1], lightPtr->attenuation[2] }
                                                          , lightPtr->falloffExponent, cutoffAngle );
  readObject( spotLight, lightPtr );
  readNode( spotLight, lightPtr );
  readLightSource_nbf_12( spotLight, lightPtr );

  mapObject( offset, spotLight );
  return( spotLight );
}

void DPBFLoader::readLightSource_nbf_53( LightSourceSharedPtr const& dst, const NBFLightSource_nbf_53 * src )
{
  dst->setShadowCasting(!!src->castShadow);
  dst->setEnabled( !!src->enabled );
}

void DPBFLoader::readLightSource_nbf_52( LightSourceSharedPtr const& dst, const NBFLightSource_nbf_52 * src )
{
  dst->setShadowCasting(!!src->castShadow);
  dst->setEnabled( !!src->enabled );
}

void DPBFLoader::readLightSource_nbf_50( LightSourceSharedPtr const& dst, const NBFLightSource_nbf_50 * src )
{
  dst->setShadowCasting(!!src->castShadow);
  dst->setEnabled( ((m_nbfMajor<0x34)||((m_nbfMajor==0x34)&&(m_nbfMinor<0x02))) ? true : !!src->enabled );
}

void DPBFLoader::readLightSource_nbf_12( LightSourceSharedPtr const& dst, const NBFLightSource_nbf_12 * src )
{
  dst->setShadowCasting(!!src->castShadow);
}

void DPBFLoader::loadStateAttribute_nbf_54(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFStateAttribute_nbf_54> attribPtr(m_fm, callback(), offset);

    DP_ASSERT(   attribPtr->objectCode==DPBFCode::ALPHA_TEST_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::BLEND_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::CGFX
              || attribPtr->objectCode==DPBFCode::DEPTH_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::FACE_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::LIGHTING_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::LINE_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::MATERIAL
              || attribPtr->objectCode==DPBFCode::POINT_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::RTBUFFER_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::RTFX
              || attribPtr->objectCode==DPBFCode::STENCIL_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::TEXTURE_ATTRIBUTE
              || attribPtr->objectCode==DPBFCode::UNLIT_COLOR_ATTRIBUTE
              || attribPtr->objectCode>=DPBFCode::CUSTOM_OBJECT );

    switch ( attribPtr->objectCode )
    {
      case DPBFCode::ALPHA_TEST_ATTRIBUTE:  DP_VERIFY(loadAlphaTestAttribute_nbf_54(offset));     break;
      case DPBFCode::BLEND_ATTRIBUTE:       DP_VERIFY(loadBlendAttribute_nbf_54(offset));         break;
      case DPBFCode::CGFX:                  break;  // ignore
      case DPBFCode::DEPTH_ATTRIBUTE:       break;  // ignore
      case DPBFCode::FACE_ATTRIBUTE:        DP_VERIFY((this->*m_pfnLoadFaceAttribute)(offset));   break;
      case DPBFCode::LIGHTING_ATTRIBUTE:    DP_VERIFY(loadLightingAttribute_nbf_54(offset));      break;
      case DPBFCode::LINE_ATTRIBUTE:        DP_VERIFY(loadLineAttribute_nbf_54(offset));          break;
      case DPBFCode::MATERIAL:              DP_VERIFY((this->*m_pfnLoadMaterial)(offset));        break;
      case DPBFCode::POINT_ATTRIBUTE:       DP_VERIFY(loadPointAttribute_nbf_54(offset));         break;
      case DPBFCode::RTBUFFER_ATTRIBUTE:    break; // ignore
      case DPBFCode::RTFX:                  break; // ignore
      case DPBFCode::STENCIL_ATTRIBUTE:     break; // ignore
      case DPBFCode::TEXTURE_ATTRIBUTE:     DP_VERIFY(loadTextureAttribute_nbf_54(offset));       break;
      case DPBFCode::UNLIT_COLOR_ATTRIBUTE: DP_VERIFY(loadUnlitColorAttribute_nbf_54(offset));    break;
      // custom object handling
      default: mapObject(offset, loadCustomObject(attribPtr->objectCode, offset));
    }
    // attribute should have been mapped if we get here
  }
  else
  {
    std::map<uint_t,dp::sg::core::PipelineDataSharedPtr>::const_iterator it = m_materialToPipelineData.find( offset );
    if ( it != m_materialToPipelineData.end() )
    {
      m_pipelineData = it->second;
    }
  }
}

ParameterGroupDataSharedPtr DPBFLoader::loadAlphaTestAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFAlphaTestAttribute_nbf_54> ataPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(ataPtr->objectCode==DPBFCode::ALPHA_TEST_ATTRIBUTE);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  readObject( materialData, ataPtr );

  DP_VERIFY( materialData->setParameter<int>( "alphaFunction", ataPtr->alphaFunction ) );
  DP_VERIFY( materialData->setParameter( "alphaThreshold", ataPtr->threshold ) );

  mapObject( offset, materialData );
  return( materialData );
}

dp::sg::core::PipelineDataSharedPtr DPBFLoader::loadBlendAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFBlendAttribute_nbf_54> baPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(baPtr->objectCode==DPBFCode::BLEND_ATTRIBUTE);

  enum class BlendFunction
  {
    ZERO                           //!< (0,0,0,0); available for source and destination
  , ONE                            //!< (1,1,1,1); available for source and destination
  , DESTINATION_COLOR              //!< (Rd,Gd,Bd,Ad); available for source only
  , SOURCE_COLOR                   //!< (Rs,Gs,Bs,As); available for destination only
  , ONE_MINUS_DESTINATION_COLOR    //!< (1,1,1,1)-(Rd,Gd,Bd,Ad); available for source only
  , ONE_MINUS_SOURCE_COLOR         //!< (1,1,1,1)-(Rs,Gs,Bs,As); available for destination only
  , SOURCE_ALPHA                   //!< (As,As,As,As); available for source and destination
  , ONE_MINUS_SOURCE_ALPHA         //!< (1,1,1,1)-(As,As,As,As); available for source and destination
  , DESTINATION_ALPHA              //!< (Ad,Ad,Ad,Ad); available for source and destination
  , ONE_MINUS_DESTINATION_ALPHA    //!< (1,1,1,1)-(Ad,Ad,Ad,Ad); available for source and destination
  , SOURCE_ALPHA_SATURATE          //!< (f,f,f,1) with f=min(As,1-Ad); available for source only
  };

  BlendFunction src = (BlendFunction)baPtr->sourceFunction;
  BlendFunction dst = (BlendFunction)baPtr->destinationFunction;

  bool transparent =  ( src == BlendFunction::DESTINATION_COLOR )
                  ||  ( src == BlendFunction::ONE_MINUS_DESTINATION_COLOR )
                  ||  ( src == BlendFunction::DESTINATION_ALPHA )
                  ||  ( src == BlendFunction::ONE_MINUS_DESTINATION_ALPHA )
                  ||  ( src == BlendFunction::SOURCE_ALPHA_SATURATE )
                  ||  ( src != BlendFunction::ZERO );
  getMaterialPipeline()->setTransparent( transparent );

  return( m_pipelineData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadFaceAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFFaceAttribute_nbf_54> faPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(faPtr->objectCode==DPBFCode::FACE_ATTRIBUTE);

  if ( ( ( 0x4d < m_nbfMajor ) || ( ( 0x4d == m_nbfMajor ) && ( 0x00 < m_nbfMinor ) ) ) && faPtr->faceWindingCCW )
  {
    ParameterGroupDataSharedPtr geometryData = getMaterialParameterGroup( "standardGeometryParameters" );
    DP_VERIFY( geometryData->setParameter( "faceWindingCCW", !!faPtr->faceWindingCCW ) );
  }

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  DP_VERIFY( materialData->setParameter( "twoSidedLighting", !!faPtr->twoSidedLighting ) );

  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadFaceAttribute_nbf_b(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFFaceAttribute_nbf_b> faPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(faPtr->objectCode==DPBFCode::FACE_ATTRIBUTE);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  DP_VERIFY( materialData->setParameter( "twoSidedLighting", !!faPtr->twoSidedLighting ) );

  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadLightingAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFLightingAttribute_nbf_54> laPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(laPtr->objectCode==DPBFCode::LIGHTING_ATTRIBUTE);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  DP_VERIFY( materialData->setParameter( "lightingEnabled", !!laPtr->enabled ) );

  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadLineAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFLineAttribute_nbf_54> laPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(laPtr->objectCode==DPBFCode::LINE_ATTRIBUTE);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  DP_VERIFY( materialData->setParameter( "lineStippleFactor", dp::checked_cast<uint16_t>(laPtr->stippleFactor) ) );
  DP_VERIFY( materialData->setParameter( "lineStipplePattern", dp::checked_cast<uint16_t>(laPtr->stipplePattern) ) );

  ParameterGroupDataSharedPtr geometryData = getMaterialParameterGroup( "standardGeometryParameters" );
  DP_VERIFY( geometryData->setParameter( "lineWidth", laPtr->width ) );

  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadUnlitColorAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFUnlitColorAttribute_nbf_54> ucPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT( ucPtr->objectCode==DPBFCode::UNLIT_COLOR_ATTRIBUTE);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  DP_VERIFY( materialData->setParameter( "unlitColor", convert<Vec4f>( ucPtr->color ) ) );

  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::getMaterialParameterGroup( const string & name )
{
  ParameterGroupDataSharedPtr pgd;
  {
    dp::sg::core::PipelineDataSharedPtr const& me = getMaterialPipeline();
    pgd = me->findParameterGroupData( name );
    if ( !pgd )
    {
      const dp::fx::EffectSpecSharedPtr es = me->getEffectSpec();
      dp::fx::EffectSpec::iterator it = es->findParameterGroupSpec( name );
      DP_ASSERT( it != es->endParameterGroupSpecs() );
      pgd = ParameterGroupData::create( *it);
      me->setParameterGroupData( it, pgd );
    }
  }
  return( pgd );
}

ParameterGroupDataSharedPtr DPBFLoader::loadMaterial_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFMaterial_nbf_54> matPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(matPtr->objectCode==DPBFCode::MATERIAL);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  readObject( materialData, matPtr );

  DP_VERIFY( materialData->setParameter( "frontAmbientColor", convert(matPtr->frontAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "frontDiffuseColor", convert(matPtr->frontDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "frontEmissiveColor", convert(matPtr->frontEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "frontIOR", matPtr->frontIndexOfRefraction ) );
  DP_VERIFY( materialData->setParameter( "frontOpacityColor", convert(matPtr->frontOpacityColor) ) );
  DP_VERIFY( materialData->setParameter( "frontReflectivityColor", convert(matPtr->frontReflectivityColor) ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularColor", convert(matPtr->frontSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularExponent", matPtr->frontSpecularExponent ) );

  DP_VERIFY( materialData->setParameter( "backAmbientColor", convert(matPtr->backAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "backDiffuseColor", convert(matPtr->backDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "backEmissiveColor", convert(matPtr->backEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "backIOR", matPtr->backIndexOfRefraction ) );
  DP_VERIFY( materialData->setParameter( "backOpacityColor", convert(matPtr->backOpacityColor) ) );
  DP_VERIFY( materialData->setParameter( "backReflectivityColor", convert(matPtr->backReflectivityColor) ) );
  DP_VERIFY( materialData->setParameter( "backSpecularColor", convert(matPtr->backSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "backSpecularExponent", matPtr->backSpecularExponent ) );

  bool transparent =    ( matPtr->frontOpacityColor[0] != 1.0f )
                    ||  ( matPtr->frontOpacityColor[1] != 1.0f )
                    ||  ( matPtr->frontOpacityColor[2] != 1.0f )
                    ||  ( matPtr->backOpacityColor[0] != 1.0f )
                    ||  ( matPtr->backOpacityColor[1] != 1.0f )
                    ||  ( matPtr->backOpacityColor[2] != 1.0f );
  getMaterialPipeline()->setTransparent( transparent );

  mapObject( offset, materialData );
  m_materialToPipelineData[offset] = m_pipelineData;
  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadMaterial_nbf_40(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFMaterial_nbf_40> matPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(matPtr->objectCode==DPBFCode::MATERIAL);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  readObject( materialData, matPtr );

  DP_VERIFY( materialData->setParameter( "frontAmbientColor", convert(matPtr->frontAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "frontDiffuseColor", convert(matPtr->frontDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "frontEmissiveColor", convert(matPtr->frontEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "frontIOR", matPtr->frontIndexOfRefraction ) );
  DP_VERIFY( materialData->setParameter( "frontOpacity", matPtr->frontOpacity ) );
  DP_VERIFY( materialData->setParameter( "frontReflectivity", matPtr->frontReflectivity ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularColor", convert(matPtr->frontSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularExponent", matPtr->frontSpecularExponent ) );

  DP_VERIFY( materialData->setParameter( "backAmbientColor", convert(matPtr->backAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "backDiffuseColor", convert(matPtr->backDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "backEmissiveColor", convert(matPtr->backEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "backIOR", matPtr->backIndexOfRefraction ) );
  DP_VERIFY( materialData->setParameter( "backOpacity", matPtr->backOpacity ) );
  DP_VERIFY( materialData->setParameter( "backReflectivity", matPtr->backReflectivity ) );
  DP_VERIFY( materialData->setParameter( "backSpecularColor", convert(matPtr->backSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "backSpecularExponent", matPtr->backSpecularExponent ) );

  bool transparent = ( matPtr->frontOpacity != 1.0f ) || ( matPtr->backOpacity != 1.0f );
  getMaterialPipeline()->setTransparent( transparent );

  mapObject( offset, materialData );
  m_materialToPipelineData[offset] = m_pipelineData;
  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadMaterial_nbf_3f(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFMaterial_nbf_3f> matPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(matPtr->objectCode==DPBFCode::MATERIAL);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  readObject( materialData, matPtr );

  DP_VERIFY( materialData->setParameter( "frontAmbientColor", convert(matPtr->frontAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "frontDiffuseColor", convert(matPtr->frontDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "frontEmissiveColor", convert(matPtr->frontEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "frontOpacity", matPtr->frontOpacity ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularColor", convert(matPtr->frontSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularExponent", matPtr->frontSpecularExponent ) );

  DP_VERIFY( materialData->setParameter( "backAmbientColor", convert(matPtr->backAmbientColor) ) );
  DP_VERIFY( materialData->setParameter( "backDiffuseColor", convert(matPtr->backDiffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "backEmissiveColor", convert(matPtr->backEmissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "backOpacity", matPtr->backOpacity ) );
  DP_VERIFY( materialData->setParameter( "backSpecularColor", convert(matPtr->backSpecularColor) ) );
  DP_VERIFY( materialData->setParameter( "backSpecularExponent", matPtr->backSpecularExponent ) );

  bool transparent = ( matPtr->frontOpacity != 1.0f ) || ( matPtr->backOpacity != 1.0f );
  getMaterialPipeline()->setTransparent( transparent );

  mapObject( offset, materialData );
  m_materialToPipelineData[offset] = m_pipelineData;
  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadMaterial_nbf_a(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFMaterial_nbf_a> matPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(matPtr->objectCode==DPBFCode::MATERIAL);

  ParameterGroupDataSharedPtr materialData = getMaterialParameterGroup( "standardMaterialParameters" );
  readObject( materialData, matPtr );

  DP_VERIFY( materialData->setParameter( "frontAmbientColor", convert(matPtr->ambientColor) ) );
  DP_VERIFY( materialData->setParameter( "frontDiffuseColor", convert(matPtr->diffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "frontEmissiveColor", convert(matPtr->emissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "frontOpacity", matPtr->opacity ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularColor", convert(matPtr->specularColor) ) );
  DP_VERIFY( materialData->setParameter( "frontSpecularExponent", matPtr->specularExponent ) );

  DP_VERIFY( materialData->setParameter( "backAmbientColor", convert(matPtr->ambientColor) ) );
  DP_VERIFY( materialData->setParameter( "backDiffuseColor", convert(matPtr->diffuseColor) ) );
  DP_VERIFY( materialData->setParameter( "backEmissiveColor", convert(matPtr->emissiveColor) ) );
  DP_VERIFY( materialData->setParameter( "backOpacity", matPtr->opacity ) );
  DP_VERIFY( materialData->setParameter( "backSpecularColor", convert(matPtr->specularColor) ) );
  DP_VERIFY( materialData->setParameter( "backSpecularExponent", matPtr->specularExponent ) );

  bool transparent = ( matPtr->opacity != 1.0f );
  getMaterialPipeline()->setTransparent( transparent );

  mapObject( offset, materialData );
  m_materialToPipelineData[offset] = m_pipelineData;
  return( materialData );
}

ParameterGroupDataSharedPtr DPBFLoader::loadPointAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFPointAttribute_nbf_54> paPtr(m_fm, callback(), offset);
  // undefined behaviour if called for other objects!
  DP_ASSERT(paPtr->objectCode==DPBFCode::POINT_ATTRIBUTE);

  ParameterGroupDataSharedPtr geometryData = getMaterialParameterGroup( "standardGeometryParameters" );
  DP_VERIFY( geometryData->setParameter( "pointSize", paPtr->size ) );

  return( geometryData );
}

dp::sg::core::PipelineDataSharedPtr DPBFLoader::loadTextureAttribute_nbf_54(uint_t offset)
{
  // this should have been caught by upper layers
  DP_ASSERT(m_offsetObjectMap.find(offset)==m_offsetObjectMap.end());

  Offset_AutoPtr<NBFTextureAttribute_nbf_54> texPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(texPtr->objectCode==DPBFCode::TEXTURE_ATTRIBUTE);

  dp::sg::core::PipelineDataSharedPtr textures = dp::sg::core::PipelineData::create( getStandardMaterialSpec() );
  readObject( textures, texPtr );

  Offset_AutoPtr<texBinding_t_nbf_54> bindings(m_fm, callback(), texPtr->bindings, texPtr->numBindings);
  for ( unsigned int i=0; i<texPtr->numBindings; ++i )
  {
    ParameterGroupDataSharedPtr item((this->*m_pfnLoadTextureAttributeItem)(bindings[i].texAttribItem));
    if ( item ) // NOTE: binding an invalid item is an error!
    {
      DP_VERIFY( textures->setParameterGroupData( item ) );
    }
  }

  mapObject(offset, textures);
  return textures;
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_e(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_e> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_f(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_f> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_12(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_12> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_20(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_20> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_36(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_36> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_4b(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_4b> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

ParameterGroupDataSharedPtr DPBFLoader::loadTextureAttributeItem_nbf_54(uint_t offset)
{
  // do not evaluate the same offset twice!
  // this will not be catched at upper layers - we need the runtime if here!
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTextureAttributeItem_nbf_54> itemPtr(m_fm, callback(), offset);
    mapObject(offset, readTexAttribItem_nbf_54(itemPtr));
  }
  return std::static_pointer_cast<ParameterGroupData>(m_offsetObjectMap[offset]);
}

TextureHostSharedPtr DPBFLoader::loadTextureHost(uint_t offset, std::string& file)
{
  file.clear();

  // 0-offset invalid for all but NBFHeaders!
  if ( !offset )
  {
    // 0-offset is regular and means no TextureHost saved -> return NULL
    return TextureHostSharedPtr();
  }

  TextureHostSharedPtr imgHdl;
  Offset_AutoPtr<texImage_t> imgPtr(m_fm, callback(), offset);

  if ( imgPtr->file.numChars )
  { // we have a file to load from
    file = m_fileFinder.find( mapString( imgPtr->file ) );
    if ( !file.empty() )
    {
      map<string,TextureHostWeakPtr>::const_iterator it = m_textureImages.find( file );
      if ( it != m_textureImages.end() )
      {
        imgHdl = std::static_pointer_cast<TextureHost>(it->second.lock());
      }
      else
      {
        imgHdl = dp::sg::io::loadTextureHost(file, m_fileFinder);
        imgHdl->setTextureTarget( (TextureTarget)imgPtr->target );
        m_textureImages[file] = imgHdl;
      }
    }
    else
    { // ran into problems with looking up the texture file
      // --> dismiss the item and output an error message
      INVOKE_CALLBACK(onFileNotFound(mapString(imgPtr->file)));
    }
  }
  else if ( imgPtr->pixels )
  { // create the image from lump
    imgHdl = TextureHost::create();
    imgHdl->setCreationFlags(imgPtr->flags);
    unsigned int index = imgHdl->addImage( imgPtr->width, imgPtr->height, imgPtr->depth
                                        , (Image::PixelFormat)imgPtr->pixelFormat
                                        , (Image::PixelDataType)imgPtr->dataType );
    DP_ASSERT( index != -1 );
    uint_t nbytes = imgHdl->getNumberOfBytes( index );
    DP_ASSERT( nbytes ==  numberOfComponents( (Image::PixelFormat)imgPtr->pixelFormat )
                          * sizeOfComponents( (Image::PixelDataType)imgPtr->dataType )
                          * imgPtr->width * imgPtr->height * imgPtr->depth );
    Offset_AutoPtr<ubyte_t> pixels(m_fm, callback(), imgPtr->pixels, nbytes);
    imgHdl->setImageData( index, (const void *) pixels );
    imgHdl->setTextureTarget( (TextureTarget)imgPtr->target );
  }
  return imgHdl;
}

TextureHostSharedPtr DPBFLoader::loadTextureHost_nbf_4b(uint_t offset, std::string& file)
{
  file.clear();

  // 0-offset invalid for all but NBFHeaders!
  if ( !offset )
  {
    // 0-offset is regular and means no TextureHost saved -> return NULL
    return TextureHostSharedPtr();
  }

  TextureHostSharedPtr imgHdl;
  Offset_AutoPtr<texImage_nbf_4b_t> imgPtr(m_fm, callback(), offset);

  if ( imgPtr->file.numChars )
  { // we have a file to load from
    file = m_fileFinder.find( mapString( imgPtr->file ) );
    if ( !file.empty() )
    {
      map<string,TextureHostWeakPtr>::const_iterator it = m_textureImages.find( file );
      if ( it != m_textureImages.end() )
      {
        imgHdl = std::static_pointer_cast<TextureHost>(it->second.lock());
      }
      else
      {
        imgHdl = dp::sg::io::loadTextureHost(file, m_fileFinder);
        m_textureImages[file] = imgHdl;
      }
    }
    else
    { // ran into problems with looking up the texture file
      // --> dismiss the item and output an error message
      INVOKE_CALLBACK(onFileNotFound(mapString(imgPtr->file)));
    }
  }
  else if ( imgPtr->pixels )
  { // create the image from lump
    imgHdl = TextureHost::create();
    imgHdl->setCreationFlags(imgPtr->flags);
    unsigned int index = imgHdl->addImage( imgPtr->width, imgPtr->height, imgPtr->depth
                                         , (Image::PixelFormat)imgPtr->pixelFormat
                                         , (Image::PixelDataType)imgPtr->dataType );
    DP_ASSERT( index != -1 );
    uint_t nbytes = imgHdl->getNumberOfBytes( index );
    DP_ASSERT( nbytes ==  numberOfComponents( (Image::PixelFormat)imgPtr->pixelFormat )
      * sizeOfComponents( (Image::PixelDataType)imgPtr->dataType )
      * imgPtr->width * imgPtr->height * imgPtr->depth );
    Offset_AutoPtr<ubyte_t> pixels(m_fm, callback(), imgPtr->pixels, nbytes);
    imgHdl->setImageData( index, (const void *) pixels );
  }
  return imgHdl;
}

void DPBFLoader::loadStateSet_nbf_54(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFStateSet_nbf_54> ssPtr(m_fm, callback(), offset);
    // undefined behavior if called for other objects!
    DP_ASSERT(ssPtr->objectCode==DPBFCode::STATE_SET);

    DP_ASSERT( !m_pipelineData );

    Offset_AutoPtr<uint_t> attribOffs(m_fm, callback(), ssPtr->stateAttribs, ssPtr->numStateAttribs);
    for ( unsigned int i=0; i<ssPtr->numStateAttribs; ++i )
    {
      loadStateAttribute_nbf_54(attribOffs[i]);
    }

    if ( m_pipelineData )
    {
      DP_ASSERT( !m_pipelineData || ( m_pipelineData->getNumberOfParameterGroupData() != 0 ) );
      m_stateSetToPipeline[offset] = m_pipelineData;
      m_pipelineData.reset();
    }
  }
}

void DPBFLoader::loadStateSet_nbf_4f(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFStateSet_nbf_4f> ssPtr(m_fm, callback(), offset);
    // undefined behavior if called for other objects!
    DP_ASSERT(ssPtr->objectCode==DPBFCode::STATE_SET);

    DP_ASSERT( !m_pipelineData );

    // StateSet specific
    Offset_AutoPtr<keyVariant_t> kvOffs(m_fm, callback(), ssPtr->keyStateVariantPairs, ssPtr->numStateVariants);

    // ignore all but the very first variant !!!
    loadStateVariant_nbf_4f( kvOffs[0].variant );

    if ( m_pipelineData )
    {
      DP_ASSERT( !m_pipelineData || ( m_pipelineData->getNumberOfParameterGroupData() != 0 ) );
      m_stateSetToPipeline[offset] = m_pipelineData;
      m_pipelineData.reset();
    }
  }
}

void DPBFLoader::loadStateSet_nbf_10(uint_t offset)
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFStateSet_nbf_10> ssPtr(m_fm, callback(), offset);
    // undefined behavior if called for other objects!
    DP_ASSERT(ssPtr->objectCode==DPBFCode::STATE_SET);

    DP_ASSERT( !m_pipelineData );

    // StateSet specific
    Offset_AutoPtr<keyVariant_t> kvOffs(m_fm, callback(), ssPtr->keyStateVariantPairs, ssPtr->numStateVariants);
    // ignore all but the very first variant !!!
    DP_ASSERT( ssPtr->numStateVariants == 1 );

    loadStateVariant_nbf_4f( kvOffs[0].variant );

    if ( m_pipelineData )
    {
      DP_ASSERT( !m_pipelineData || ( m_pipelineData->getNumberOfParameterGroupData() != 0 ) );
      m_stateSetToPipeline[offset] = m_pipelineData;
      m_pipelineData.reset();
    }
  }
}

void DPBFLoader::loadStateVariant_nbf_4f(uint_t offset)
{
  Offset_AutoPtr<NBFStateVariant_nbf_4f> svPtr(m_fm, callback(), offset);
  // undefined behavior if called for other objects!
  DP_ASSERT(svPtr->objectCode==DPBFCode::STATE_VARIANT);

  // StateVariant specific
  Offset_AutoPtr<uint_t> passOffs(m_fm, callback(), svPtr->statePasses, svPtr->numStatePasses);
  // ignore all but the very first pass !!!
  DP_ASSERT( svPtr->numStatePasses == 1 );

  Offset_AutoPtr<NBFStatePass_nbf_4f> spPtr(m_fm, callback(), passOffs[0]);
  // undefined behavior if called for other objects!
  DP_ASSERT(spPtr->objectCode==DPBFCode::STATE_PASS);

  // StatePass specific
  Offset_AutoPtr<uint_t> attribOffs(m_fm, callback(), spPtr->stateAttribs, spPtr->numStateAttribs);
  for ( unsigned int i=0; i<spPtr->numStateAttribs; ++i )
  {
    loadStateAttribute_nbf_54(attribOffs[i]);
  }
}

template<typename ObjectType>
void createObject( typename ObjectTraits<ObjectType>::SharedPtr & objHdl, PrimitiveType pt )
{
  objHdl = ObjectType::create();
}

template<>
void createObject<Primitive>( ObjectTraits<Primitive>::SharedPtr & objHdl, PrimitiveType pt )
{
  objHdl = Primitive::create( pt );
}

template <typename ObjectType, typename NBFObjectType>
bool DPBFLoader::loadSharedObject( typename ObjectTraits<ObjectType>::SharedPtr & objHdl
                                , Offset_AutoPtr<NBFObjectType>& objPtr, PrimitiveType pt )
{
  if (  objPtr->isShared
     && m_sharedObjectsMap.find(objPtr->objectDataID) != m_sharedObjectsMap.end()
     )
  { // copy construct the object
    DP_ASSERT( std::dynamic_pointer_cast<ObjectType>(m_sharedObjectsMap[objPtr->objectDataID]) );
    dp::sg::core::ObjectSharedPtr obj = std::static_pointer_cast<dp::sg::core::Object>(m_sharedObjectsMap[objPtr->objectDataID]->clone());
    objHdl = std::static_pointer_cast<ObjectType>(obj);
    // override general object data!
    // objects do not share general object data, e.g. the name, even if they are shared!
    readObject( objHdl, objPtr);
    return true;
  }

  // at least not recorded yet -> default construct
  createObject<ObjectType>( objHdl, pt );

  // NOTE: we need to read in the general object data, e.g the name, before the pointer
  // to the NBF object might be remapped! this is because objects do not share general
  // object data, even if they are shared!
  readObject( objHdl, objPtr);

  if ( objPtr->isShared )
  {
    // register shared object
    m_sharedObjectsMap[objPtr->objectDataID] = objHdl;

    if ( objPtr->sourceObject )
    { // need to remap current objectPtr to its source object, because objPtr currently
      // refers to the general object data only, not to type-specific data!
      objPtr.reset(objPtr->sourceObject);
    }
  }
  return false;
}

void DPBFLoader::readObject( ObjectSharedPtr const& dst, const NBFObject * src)
{
  if( m_nbfMajor > 0x3d
    || (m_nbfMajor == 0x3d && m_nbfMinor >= 0x05) )
  {
    // objectName and objectAnno members are offsets to str_t
    if ( src->objectName )
    {
      Offset_AutoPtr<str_t> name(m_fm, callback(), src->objectName);
      if ( name->numChars  )
      {
        dst->setName(mapString(*name));
      }
    }

    if ( src->objectAnno )
    {
      Offset_AutoPtr<str_t> anno(m_fm, callback(), src->objectAnno);
      if ( anno->numChars )
      {
        dst->setAnnotation(mapString(*anno));
      }
    }

    // only use lower 24 bits, but dont mask for now
    dst->setHints( src->hints );
  }
  else
  {
    // use legacy data structure
    const NBFObject_3d_04 * _src =
                          reinterpret_cast< const NBFObject_3d_04 * >( src );

    if (  m_nbfMajor < 0x3d
       || (m_nbfMajor==0x3d && m_nbfMinor < 0x02) )
    {
      // objectName was a str_t for all version < 61.2!
      // cast address of objectName to str_t* to get the offsets right!
      str_t * name = (str_t *)&_src->objectName;
      if ( name->numChars )
      {
        dst->setName(mapString(*name)); // call mapString with *name,
                                    // so the compiler finds the right overload
      }
    }
    else if (  m_nbfMajor==0x3d
         && (m_nbfMinor==0x02 || m_nbfMinor==0x03) )
    {
      // name and annotation were stored as a sstr_t with
      // intermediate versions 61.2 and 61.3
      //
      sstr_t * name = (sstr_t *)&_src->objectName;
      if ( name->numChars )
      {
        dst->setName(mapString(*name));
      }

      sstr_t * anno = (sstr_t *)&_src->objectAnno;
      if ( anno->numChars )
      {
        dst->setAnnotation(mapString(*anno));
      }
    }
  }
}

void  DPBFLoader::readIndependentPrimitiveSet( PrimitiveSharedPtr const& dst, const NBFIndependentPrimitiveSet *src )
{
  // read object related data - is this necessary??
  readObject( dst, src);

  // the only thing we care about is the VAS from the PrimitiveSet, so we just read that out here
  VertexAttributeSetSharedPtr vas( (this->*m_pfnLoadVertexAttributeSet)(src->vertexAttributeSet) );
  if ( vas )
  {
    dst->setVertexAttributeSet( vas );
  }

  DP_ASSERT(src->numIndices);
  Offset_AutoPtr<uint_t> indices(m_fm, callback(), src->indices, src->numIndices);
  IndexSetSharedPtr iset( IndexSet::create() );
  iset->setData( indices, src->numIndices );
  dst->setIndexSet( iset );
}


void DPBFLoader::readPrimitive( PrimitiveSharedPtr const& dst, const NBFPrimitive * src )
{
  // read object related data
  readObject( dst, src);

  VertexAttributeSetSharedPtr vas( (this->*m_pfnLoadVertexAttributeSet)(src->vertexAttributeSet) );
  if ( vas )
  {
    dst->setVertexAttributeSet( vas );
  }

  DP_ASSERT( !!src->indexSet == !!(src->renderFlags & Primitive::DRAW_INDEXED) );
  if( src->renderFlags & Primitive::DRAW_INDEXED )
  {
    IndexSetSharedPtr is( loadIndexSet( src->indexSet ) );
    if ( is )
    {
      dst->setIndexSet( is );
    }
  }

  dst->setElementRange( src->elementOffset, src->elementCount );
  dst->setInstanceCount( src->instanceCount );

  if ( dst->getPrimitiveType() == PrimitiveType::PATCHES )
  {
    if ( ( 0x54 < m_nbfMajor ) || ( ( m_nbfMajor == 0x54 ) && ( 0x02 < m_nbfMinor ) ) )
    {
      dst->setPatchesOrdering( (PatchesOrdering)src->patchesOrdering );
      dst->setPatchesSpacing( (PatchesSpacing)src->patchesSpacing );
    }
  }
}

void DPBFLoader::readPrimitive_nbf_4d( PrimitiveSharedPtr const& dst, const NBFPrimitive_nbf_4d * src )
{
  // read object related data
  readObject( dst, src);

  VertexAttributeSetSharedPtr vas( (this->*m_pfnLoadVertexAttributeSet)(src->vertexAttributeSet) );
  if ( vas )
  {
    dst->setVertexAttributeSet( vas );
  }

  if( src->renderFlags & Primitive::DRAW_INDEXED )
  {
    IndexSetSharedPtr iset( IndexSet::create() );

    unsigned int byteSize = dp::checked_cast<unsigned int>(dp::getSizeOf( convertDataType(src->indexSet.dataType) ) * src->indexSet.numberOfIndices);
    Offset_AutoPtr<byte_t> indicesPtr( m_fm, callback(), src->indexSet.idata, byteSize );

    iset->setData( indicesPtr, src->indexSet.numberOfIndices, convertDataType(src->indexSet.dataType), src->indexSet.primitiveRestartIndex );

    dst->setIndexSet( iset );
  }

  dst->setElementRange( src->elementOffset, src->elementCount );
  dst->setInstanceCount( src->instanceCount );
}

void DPBFLoader::readPrimitiveSet( PrimitiveSharedPtr const& dst, const NBFPrimitiveSet * src )
{
  // read object related data
  readObject( dst, src);

  VertexAttributeSetSharedPtr vas( (this->*m_pfnLoadVertexAttributeSet)(src->vertexAttributeSet) );
  if ( vas )
  {
    dst->setVertexAttributeSet( vas );
  }
}

PrimitiveSharedPtr DPBFLoader::loadPrimitive( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFPrimitive> primPtr(m_fm, callback(), offset);

    PrimitiveSharedPtr primHdl;
    if ( ( (PrimitiveType)primPtr->primitiveType == PrimitiveType::PATCHES ) &&
         ( ( 0x54 < m_nbfMajor ) || ( ( m_nbfMajor == 0x54 ) && ( 0x02 < m_nbfMinor ) ) ) )
    {
      primHdl = Primitive::create( (PatchesType)primPtr->patchesType, (PatchesMode)primPtr->patchesMode );
    }
    else
    {
      primHdl = Primitive::create( (PrimitiveType)primPtr->primitiveType );
    }
    readPrimitive( primHdl, primPtr );

    mapObject(offset, primHdl);
  }

  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadPrimitive_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFPrimitive_nbf_4d> primPtr(m_fm, callback(), offset);

    PrimitiveSharedPtr primHdl = Primitive::create( (PrimitiveType)primPtr->primitiveType );
    readPrimitive_nbf_4d( primHdl, primPtr );

    mapObject(offset, primHdl);
  }

  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadPatches_nbf_47( uint_t offset )
{
  // patches went away in version 0x48
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFPatches_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->verticesPerPatch == 16 );
    PrimitiveSharedPtr patchesHdl = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readIndependentPrimitiveSet( patchesHdl, patchesPtr );

    mapObject(offset, patchesHdl);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches_nbf_47( uint_t offset )
{
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readIndependentPrimitiveSet( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches4x4_nbf_47( uint_t offset )
{
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches4x4_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES_4X4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readIndependentPrimitiveSet( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadRectPatches_nbf_47( uint_t offset )
{
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFRectPatches_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::RECT_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( ( patchesPtr->width == 4 ) && ( patchesPtr->height == 4 ) );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readIndependentPrimitiveSet( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches_nbf_47( uint_t offset )
{
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readIndependentPrimitiveSet( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches4_nbf_47( uint_t offset )
{
  DP_ASSERT( m_nbfMajor < 0x48 );

  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches4_nbf_47> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES_4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readIndependentPrimitiveSet( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches_nbf_4d> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive_nbf_4d( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches4x4( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches4x4> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES_4X4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadQuadPatches4x4_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFQuadPatches4x4_nbf_4d> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::QUAD_PATCHES_4X4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive_nbf_4d( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadRectPatches( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFRectPatches> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::RECT_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( ( patchesPtr->width == 4 ) && ( patchesPtr->height == 4 ) );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadRectPatches_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFRectPatches_nbf_4d> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::RECT_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( ( patchesPtr->width == 4 ) && ( patchesPtr->height == 4 ) );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_QUADS, PatchesMode::QUADS );
    readPrimitive_nbf_4d( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readPrimitive( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches_nbf_4d> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    DP_ASSERT( patchesPtr->size == 4 );
    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readPrimitive_nbf_4d( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches4( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches4> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES_4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readPrimitive( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

PrimitiveSharedPtr DPBFLoader::loadTriPatches4_nbf_4d( uint_t offset )
{
  if ( m_offsetObjectMap.find(offset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFTriPatches4_nbf_4d> patchesPtr(m_fm, callback(), offset);
    DP_ASSERT(    patchesPtr->objectCode == DPBFCode::TRI_PATCHES_4
              ||  patchesPtr->objectCode >= DPBFCode::CUSTOM_OBJECT );

    PrimitiveSharedPtr primitive = Primitive::create( PatchesType::CUBIC_BEZIER_TRIANGLES, PatchesMode::TRIANGLES );
    readPrimitive_nbf_4d( primitive, patchesPtr );

    mapObject(offset, primitive);
  }
  return(std::static_pointer_cast<Primitive>(m_offsetObjectMap[offset]));
}

void DPBFLoader::readVertexAttributeSet( VertexAttributeSetSharedPtr const& dst, const NBFVertexAttributeSet * src )
{
  // vertex attribute specific
  for ( uint_t i=0; i<static_cast<unsigned int>(VertexAttributeSet::AttributeID::VERTEX_ATTRIB_COUNT); ++i )
  {
    VertexAttributeSet::AttributeID id = static_cast<VertexAttributeSet::AttributeID>(i);
    if ( src->vattribs[i].numVData )
    {
      uint_t sizeofVertex = dp::checked_cast<uint_t>(src->vattribs[i].size * dp::getSizeOf( convertDataType(src->vattribs[i].type) ));
      Offset_AutoPtr<byte_t> vdata( m_fm, callback(), src->vattribs[i].vdata,
        src->vattribs[i].numVData * sizeofVertex );

      dst->setVertexData( id, src->vattribs[i].size, convertDataType(src->vattribs[i].type),
        vdata, 0, src->vattribs[i].numVData );

      // enable for rendering?
      DP_ASSERT(!(src->enableFlags & (1<<i)) || !(src->enableFlags & (1<<(i+16))));
      dst->setEnabled(id, !!(src->enableFlags & (1<<i))); // conventional attrib

      id = static_cast<VertexAttributeSet::AttributeID>(i+16);
      // set the normalize enable flag for the aliased generic attribute only
      dst->setNormalizeEnabled(id, !!(src->normalizeEnableFlags & (1<<(i+16))));
      dst->setEnabled(id, !!(src->enableFlags & (1<<(i+16)))); // generic attrib
    }
  }
}

IndexSetSharedPtr DPBFLoader::loadIndexSet( uint_t offset )
{
  if ( m_offsetObjectMap.find( offset ) == m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFIndexSet> isPtr( m_fm, callback(), offset );
    DP_ASSERT( isPtr->objectCode == DPBFCode::INDEX_SET );

    IndexSetSharedPtr iset;
    if ( !loadSharedObject<IndexSet>( iset, isPtr ) )
    {
      unsigned int byteSize = dp::checked_cast<uint_t>(dp::getSizeOf( convertDataType(isPtr->dataType) ) * isPtr->numberOfIndices);
      Offset_AutoPtr<byte_t> indicesPtr( m_fm, callback(), isPtr->idata, byteSize );

      iset->setData( indicesPtr, isPtr->numberOfIndices, convertDataType(isPtr->dataType), isPtr->primitiveRestartIndex );
    }
    mapObject( offset, iset );
  }
  return std::static_pointer_cast<IndexSet>(m_offsetObjectMap[offset]);
}

VertexAttributeSetSharedPtr DPBFLoader::loadVertexAttributeSet( uint_t vasOffset )
{
  if ( m_offsetObjectMap.find(vasOffset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFVertexAttributeSet> vasPtr(m_fm, callback(), vasOffset);
    DP_ASSERT( vasPtr->objectCode == DPBFCode::VERTEX_ATTRIBUTE_SET );

    VertexAttributeSetSharedPtr vash;
    if ( !loadSharedObject<VertexAttributeSet>( vash, vasPtr ) )
    readVertexAttributeSet( vash, vasPtr );
    mapObject(vasOffset, vash);
  }
  return std::static_pointer_cast<VertexAttributeSet>(m_offsetObjectMap[vasOffset]);
}

VertexAttributeSetSharedPtr DPBFLoader::loadVertexAttributeSet_nbf_54( uint_t vasOffset )
{
  if ( m_offsetObjectMap.find(vasOffset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFVertexAttributeSet> vasPtr(m_fm, callback(), vasOffset);

    if ( vasPtr->objectCode == DPBFCode::VERTEX_ATTRIBUTE_SET )
    {
      VertexAttributeSetSharedPtr vash;
      if ( !loadSharedObject<VertexAttributeSet>( vash, vasPtr ) )
      readVertexAttributeSet( vash, vasPtr );
      mapObject(vasOffset, vash);
    }
    else
    {
      DP_ASSERT( vasPtr->objectCode == DPBFCode::ANIMATED_VERTEX_ATTRIBUTE_SET );
      Offset_AutoPtr<NBFAnimatedVertexAttributeSet_nbf_54> avasPtr( m_fm, callback(), vasOffset );

      VertexAttributeSetSharedPtr vertexAttributeSet;
      if ( !loadSharedObject<VertexAttributeSet>( vertexAttributeSet, avasPtr ) )
      readVertexAttributeSet( vertexAttributeSet, avasPtr );
      mapObject(vasOffset, vertexAttributeSet);
    }
  }
  return std::static_pointer_cast<VertexAttributeSet>(m_offsetObjectMap[vasOffset]);
}

VertexAttributeSetSharedPtr DPBFLoader::loadVertexAttributeSet_nbf_3a( uint_t vasOffset )
{
  if ( m_offsetObjectMap.find(vasOffset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFVertexAttributeSet> vasPtr(m_fm, callback(), vasOffset);
    // undefined behavior if called for other objects!
    DP_ASSERT(vasPtr->objectCode==DPBFCode::VERTEX_ATTRIBUTE_SET);
    VertexAttributeSetSharedPtr hvas(VertexAttributeSet::create());

    // consider shared objects
    if ( !loadSharedObject<VertexAttributeSet>(hvas, vasPtr) )
    {
      readObject( hvas, vasPtr); // common object data

      // vertex attribute specific
      for ( uint_t i=0; i<static_cast<unsigned int>(VertexAttributeSet::AttributeID::VERTEX_ATTRIB_COUNT); ++i )
      {
        VertexAttributeSet::AttributeID id = static_cast<VertexAttributeSet::AttributeID>(i);
        if ( vasPtr->vattribs[i].numVData )
        {
          uint_t sizeofVertex = dp::checked_cast<uint_t>(vasPtr->vattribs[i].size * dp::getSizeOf( convertDataType(vasPtr->vattribs[i].type) ));
          Offset_AutoPtr<byte_t> vdata( m_fm, callback(), vasPtr->vattribs[i].vdata,
            vasPtr->vattribs[i].numVData * sizeofVertex );

          hvas->setVertexData( id, vasPtr->vattribs[i].size, convertDataType(vasPtr->vattribs[i].type),
            vdata, 0, vasPtr->vattribs[i].numVData );

          // enable for rendering?
          DP_ASSERT(!(vasPtr->enableFlags & (1<<i)) || !(vasPtr->enableFlags & (1<<(i+16))));
          hvas->setEnabled(id, !!(vasPtr->enableFlags & (1<<i))); // conventional attrib

          id = static_cast<VertexAttributeSet::AttributeID>(i+16);
          // set the normalize enable flag for the aliased generic attribute only
          hvas->setNormalizeEnabled(id, !!(vasPtr->normalizeEnableFlags & (1+16)));
          hvas->setEnabled(id, !!(vasPtr->enableFlags & (1<<(i+16)))); // generic attrib
        }
      }
    }
    mapObject(vasOffset, hvas);
  }
  return std::static_pointer_cast<VertexAttributeSet>(m_offsetObjectMap[vasOffset]);
}

VertexAttributeSetSharedPtr DPBFLoader::loadVertexAttributeSet_nbf_38( uint_t vasOffset)
{
  if ( m_offsetObjectMap.find(vasOffset)==m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFVertexAttributeSet_nbf_38> vasPtr(m_fm, callback(), vasOffset);
    // undefined behavior if called for other objects!
    DP_ASSERT(vasPtr->objectCode==DPBFCode::VERTEX_ATTRIBUTE_SET);

    VertexAttributeSetSharedPtr cvas(VertexAttributeSet::create());
    {
      readObject( cvas, vasPtr );
      // vertices
      DP_ASSERT(vasPtr->numVertices);
      Offset_AutoPtr<Vec3f> verts(m_fm, callback(), vasPtr->vertices, vasPtr->numVertices);
      cvas->setVertices(verts, vasPtr->numVertices);
      // normals
      if ( vasPtr->numNormals )
      {
        Offset_AutoPtr<Vec3f> norms(m_fm, callback(), vasPtr->normals, vasPtr->numNormals);
        cvas->setNormals(norms, vasPtr->numNormals);
      }
      // texCoords
      if ( vasPtr->numTexCoordsSets )
      {
        Offset_AutoPtr<texCoordSet_t> tcSets(m_fm, callback(), vasPtr->texCoordsSets, vasPtr->numTexCoordsSets);
        for ( unsigned int i=0; i<vasPtr->numTexCoordsSets; ++i )
        {
          if ( tcSets[i].numTexCoords )
          {
            Offset_AutoPtr<float> coords(m_fm, callback(), tcSets[i].texCoords, tcSets[i].numTexCoords * tcSets[i].coordDim);
            cvas->setVertexData( static_cast<VertexAttributeSet::AttributeID>(static_cast<unsigned int>(VertexAttributeSet::AttributeID::TEXCOORD0)+i), tcSets[i].coordDim, dp::DataType::FLOAT_32, coords, 0, tcSets[i].numTexCoords );
            cvas->setEnabled(static_cast<VertexAttributeSet::AttributeID>(static_cast<unsigned int>(VertexAttributeSet::AttributeID::TEXCOORD0)+i), true); // generic API require explicit enable
          }
        }
      }

      // colors
      if ( vasPtr->numColors )
      {
        Offset_AutoPtr<float> colors(m_fm, callback(), vasPtr->colors, vasPtr->numColors * vasPtr->colorDim);
        cvas->setVertexData( VertexAttributeSet::AttributeID::COLOR, vasPtr->colorDim, dp::DataType::FLOAT_32, colors, 0, vasPtr->numColors );
        cvas->setEnabled(VertexAttributeSet::AttributeID::COLOR, true);
      }

      // colors
      if ( vasPtr->numSecondaryColors )
      {
        Offset_AutoPtr<float> colors(m_fm, callback(), vasPtr->secondaryColors, vasPtr->numSecondaryColors * vasPtr->secondaryColorDim);
        cvas->setVertexData( VertexAttributeSet::AttributeID::SECONDARY_COLOR, vasPtr->secondaryColorDim, dp::DataType::FLOAT_32, colors, 0, vasPtr->numSecondaryColors );
        cvas->setEnabled(VertexAttributeSet::AttributeID::SECONDARY_COLOR, true); // generic API require explicit enable
      }

      // fogCoords
      if ( vasPtr->numFogCoords )
      {
        Offset_AutoPtr<float> coords(m_fm, callback(), vasPtr->fogCoords, vasPtr->numFogCoords);
        cvas->setFogCoords((const float *)coords, vasPtr->numFogCoords);
      }
    }
    mapObject(vasOffset, cvas);
    cvas;
  }
  return std::static_pointer_cast<VertexAttributeSet>(m_offsetObjectMap[vasOffset]);
}

dp::sg::core::PipelineDataSharedPtr DPBFLoader::loadPipelineData( uint_t offset )
{
  map<uint_t,ObjectSharedPtr>::const_iterator objIt = m_offsetObjectMap.find( offset );
  if ( objIt == m_offsetObjectMap.end() )
  {
    dp::sg::core::PipelineDataSharedPtr effectData;
    Offset_AutoPtr<NBFPipelineData> edPtr( m_fm, callback(), offset );
    // undefined behavior if called for other objects!
    DP_ASSERT( edPtr->objectCode == DPBFCode::PIPELINE_DATA );

    DP_VERIFY( dp::fx::EffectLibrary::instance()->loadEffects( mapString( edPtr->effectFileName ), m_fileFinder ) );
    m_currentEffectSpec = dp::fx::EffectLibrary::instance()->getEffectSpec( mapString( edPtr->effectSpecName ) );
    if ( m_currentEffectSpec )
    {
      effectData = dp::sg::core::PipelineData::create( m_currentEffectSpec );
      readObject( effectData, edPtr );

      unsigned int numParameterGroupSpecs = m_currentEffectSpec->getNumberOfParameterGroupSpecs();
      Offset_AutoPtr<uint_t> pgd( m_fm, callback(), edPtr->parameterGroupData, numParameterGroupSpecs );
      dp::fx::EffectSpec::iterator it = m_currentEffectSpec->beginParameterGroupSpecs();
      for ( unsigned int i=0 ; it != m_currentEffectSpec->endParameterGroupSpecs() ; ++it, i++ )
      {
        if ( pgd[i] )
        {
          ParameterGroupDataSharedPtr parameterGroupData = loadParameterGroupData( pgd[i] ) ;
          if ( parameterGroupData->getParameterGroupSpec()->getName() == "options" )
          {
            DP_ASSERT( ( m_nbfMajor < 0x54 ) || ( m_nbfMajor == 0x54 ) && ( m_nbfMinor < 0x04 ) );
            effectData->setTransparent( parameterGroupData->getParameter<bool>( "transparent" ) );
          }
          else
          {
            effectData->setParameterGroupData( it, loadParameterGroupData( pgd[i] ) );
          }
        }
      }

      if ( ( m_nbfMajor == 0x54 ) && ( 0x03 < m_nbfMinor ) || ( 0x054 < m_nbfMajor ) )
      {
        effectData->setTransparent( !!edPtr->transparent );
      }
      mapObject( offset, effectData );
    }
    return( effectData );
  }
  return(std::static_pointer_cast<dp::sg::core::PipelineData>(objIt->second));
}

dp::sg::core::PipelineDataSharedPtr DPBFLoader::loadPipelineData_nbf_55( uint_t offset )
{
  map<uint_t,ObjectSharedPtr>::const_iterator objIt = m_offsetObjectMap.find( offset );
  if ( objIt == m_offsetObjectMap.end() )
  {
    dp::sg::core::PipelineDataSharedPtr effectData;
    Offset_AutoPtr<NBFEffectData_nbf_55> edPtr( m_fm, callback(), offset );
    // undefined behavior if called for other objects!
    DP_ASSERT( edPtr->objectCode == DPBFCode::PIPELINE_DATA );

    m_currentEffectSpec = dp::fx::EffectLibrary::instance()->getEffectSpec( mapString( edPtr->effectSpecName ) );
    if ( m_currentEffectSpec )
    {
      effectData = dp::sg::core::PipelineData::create( m_currentEffectSpec );
      readObject( effectData, edPtr );

      unsigned int numParameterGroupSpecs = m_currentEffectSpec->getNumberOfParameterGroupSpecs();
      Offset_AutoPtr<uint_t> pgd( m_fm, callback(), edPtr->parameterGroupData, numParameterGroupSpecs );
      dp::fx::EffectSpec::iterator it = m_currentEffectSpec->beginParameterGroupSpecs();
      for ( unsigned int i=0 ; it != m_currentEffectSpec->endParameterGroupSpecs() ; ++it, i++ )
      {
        if ( pgd[i] )
        {
          ParameterGroupDataSharedPtr parameterGroupData = loadParameterGroupData( pgd[i] ) ;
          if ( parameterGroupData->getParameterGroupSpec()->getName() == "options" )
          {
            DP_ASSERT( ( m_nbfMajor < 0x54 ) || ( m_nbfMajor == 0x54 ) && ( m_nbfMinor < 0x04 ) );
            effectData->setTransparent( parameterGroupData->getParameter<bool>( "transparent" ) );
          }
          else
          {
            effectData->setParameterGroupData( it, loadParameterGroupData( pgd[i] ) );
          }
        }
      }

      if ( ( m_nbfMajor == 0x54 ) && ( 0x03 < m_nbfMinor ) || ( 0x054 < m_nbfMajor ) )
      {
        effectData->setTransparent( !!edPtr->transparent );
      }
      mapObject( offset, effectData );
    }
    return( effectData );
  }
  return(std::static_pointer_cast<dp::sg::core::PipelineData>(objIt->second));
}

ParameterGroupDataSharedPtr DPBFLoader::loadParameterGroupData( uint_t offset )
{
  map<uint_t,ObjectSharedPtr>::const_iterator objIt = m_offsetObjectMap.find( offset );
  if ( objIt == m_offsetObjectMap.end() )
  {
    ParameterGroupDataSharedPtr parameterGroupData;

    Offset_AutoPtr<NBFParameterGroupData> pgdPtr( m_fm, callback(), offset );
    // undefined behavior if called for other objects!
    DP_ASSERT( pgdPtr->objectCode == DPBFCode::PARAMETER_GROUP_DATA );

    DP_ASSERT( ( ( m_nbfMajor < 0x54 ) || ( m_nbfMajor == 0x54 ) && ( m_nbfMinor < 0x04 ) ) || ( mapString( pgdPtr->parameterGroupSpecName ) != "options" ) );

    DP_ASSERT( m_currentEffectSpec );
    dp::fx::EffectSpec::iterator it = m_currentEffectSpec->findParameterGroupSpec( mapString( pgdPtr->parameterGroupSpecName ) );
    if ( it != m_currentEffectSpec->endParameterGroupSpecs() )
    {
      const dp::fx::ParameterGroupSpecSharedPtr & pgs = *it;
      parameterGroupData = ParameterGroupData::create( pgs );
      readObject( parameterGroupData, pgdPtr );

      DP_ASSERT( pgs->getDataSize() == pgdPtr->numData );
      Offset_AutoPtr<byte_t> data( m_fm, callback(), pgdPtr->data, pgdPtr->numData );
      for ( dp::fx::ParameterGroupSpec::iterator it = pgs->beginParameterSpecs() ; it != pgs->endParameterSpecs() ; ++it )
      {
        unsigned int type = it->first.getType();
        if ( type & dp::fx::PT_SCALAR_TYPE_MASK )
        {
          parameterGroupData->setParameter( it, (const void *)&data[it->second] );
        }
        else
        {
          DP_ASSERT( type & dp::fx::PT_POINTER_TYPE_MASK );
          switch( type & dp::fx::PT_POINTER_TYPE_MASK )
          {
            case dp::fx::PT_BUFFER_PTR :
              DP_ASSERT( false );   // not yet implemented
              //parameterGroupData->setParameter( it, loadBuffer( *(uint_t*)&data[it->second] ) );
              break;
            case dp::fx::PT_SAMPLER_PTR :
              if ( data[it->second] )
              {
                parameterGroupData->setParameter( it, loadSampler( *(uint_t*)&data[it->second] ) );
              }
              break;
            default :
              DP_ASSERT( false );
              break;
          }
        }
      }
      mapObject( offset, parameterGroupData );
      return( parameterGroupData );
    }
  }
  return(std::static_pointer_cast<ParameterGroupData>(objIt->second));
}

SamplerSharedPtr DPBFLoader::loadSampler( uint_t offset )
{
  if ( m_offsetObjectMap.find( offset ) == m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFSampler> samplerPtr( m_fm, callback(), offset );
    // undefined behavior if called for other objects!
    DP_ASSERT( samplerPtr->objectCode == DPBFCode::SAMPLER );

    SamplerSharedPtr sampler = Sampler::create();
    readObject( sampler, samplerPtr );
    sampler->setTexture( readTexture( samplerPtr->texture ) );
    sampler->setBorderColor(convert<Vec4f>(samplerPtr->borderColor));
    sampler->setMagFilterMode((TextureMagFilterMode)samplerPtr->magFilter);
    sampler->setMinFilterMode((TextureMinFilterMode)samplerPtr->minFilter);
    sampler->setWrapModes( (TextureWrapMode)samplerPtr->texWrapS
                          , (TextureWrapMode)samplerPtr->texWrapT
                          , (TextureWrapMode)samplerPtr->texWrapR );
    sampler->setCompareMode( (TextureCompareMode)samplerPtr->compareMode );
    mapObject( offset, sampler );
  }
  return(std::static_pointer_cast<Sampler>(m_offsetObjectMap[offset]));
}

SamplerSharedPtr DPBFLoader::loadSampler_nbf_54( uint_t offset )
{
  if ( m_offsetObjectMap.find( offset ) == m_offsetObjectMap.end() )
  {
    Offset_AutoPtr<NBFSampler_nbf_54> samplerPtr( m_fm, callback(), offset );
    // undefined behavior if called for other objects!
    DP_ASSERT( samplerPtr->objectCode == DPBFCode::SAMPLER );

    SamplerSharedPtr sampler = Sampler::create();
    readObject( sampler, samplerPtr );
    sampler->setTexture( readTexture( samplerPtr->texture ) );
    {
      Offset_AutoPtr<NBFSamplerState_nbf_54> samplerStatePtr( m_fm, callback(), samplerPtr->samplerState );
      // undefined behavior if called for other objects!
      DP_ASSERT( samplerStatePtr->objectCode == DPBFCode::SAMPLER_STATE );

      sampler->setBorderColor(convert<Vec4f>(samplerStatePtr->borderColor));
      sampler->setMagFilterMode((TextureMagFilterMode)samplerStatePtr->magFilter);
      sampler->setMinFilterMode((TextureMinFilterMode)samplerStatePtr->minFilter);
      sampler->setWrapModes( (TextureWrapMode)samplerStatePtr->texWrapS
                            , (TextureWrapMode)samplerStatePtr->texWrapT
                            , (TextureWrapMode)samplerStatePtr->texWrapR );
      if ( ( 0x54 < m_nbfMajor ) || ( 0x54 == m_nbfMajor ) && ( 0 < m_nbfMinor ) )
      {
        sampler->setCompareMode( (TextureCompareMode)samplerStatePtr->compareMode );
      }
    }
    mapObject( offset, sampler );
  }
  return(std::static_pointer_cast<Sampler>(m_offsetObjectMap[offset]));
}

void DPBFLoader::readGroup(GroupSharedPtr const& dst, const NBFGroup * src)
{
  Offset_AutoPtr<uint_t> childOffs(m_fm, callback(), src->children, src->numChildren);
  for ( unsigned int i=0; i<src->numChildren; ++i )
  {
    NodeSharedPtr child(loadNode(childOffs[i]));
    if ( child )
    {
      dst->addChild(child);
    }
  }

  Offset_AutoPtr<plane_t> planes(m_fm, callback(), src->clipPlanes, src->numClipPlanes);
  for ( unsigned int i=0 ; i<src->numClipPlanes ; i++ )
  {
    ClipPlaneSharedPtr planePtr = ClipPlane::create();
    planePtr->setEnabled(!!planes[i].active);
    planePtr->setNormal(convert(planes[i].normal));
    planePtr->setOffset(planes[i].offset);

    dst->addClipPlane( planePtr );
  }

  Offset_AutoPtr<uint_t> lightSourceOffs(m_fm, callback(), src->lightSources, src->numLightSource);
  for ( unsigned int i=0; i<src->numLightSource; ++i )
  {
    LightSourceSharedPtr ls((this->*m_pfnLoadLightSource)(lightSourceOffs[i]));
    if ( ls )
    {
      if ( m_nbfMajor < 0x51 )
      {
        DP_ASSERT( m_lightSourceToGroup.find( ls ) == m_lightSourceToGroup.end() );
        m_lightSourceToGroup[ls] = dst;
      }
    }
  }
}

void DPBFLoader::readGroup_nbf_12(GroupSharedPtr const& dst, const NBFGroup_nbf_12 * src)
{
  Offset_AutoPtr<uint_t> childOffs(m_fm, callback(), src->children, src->numChildren);
  for ( unsigned int i=0; i<src->numChildren; ++i )
  {
    ObjectSharedPtr child(loadNode_nbf_12(childOffs[i]));
    if ( child )
    {
      DP_ASSERT( std::dynamic_pointer_cast<Node>(child) );
      dst->addChild(std::static_pointer_cast<Node>(child));
    }
  }

  Offset_AutoPtr<plane_t> planes(m_fm, callback(), src->clipPlanes, src->numClipPlanes);
  for ( unsigned int i=0 ; i<src->numClipPlanes ; i++ )
  {
    ClipPlaneSharedPtr planePtr = ClipPlane::create();
    planePtr->setEnabled(!!planes[i].active);
    planePtr->setNormal(convert(planes[i].normal));
    planePtr->setOffset(planes[i].offset);

    dst->addClipPlane( planePtr );
  }
}

void DPBFLoader::readGroup_nbf_11(GroupSharedPtr const& dst, const NBFGroup_nbf_11 * src)
{
  Offset_AutoPtr<uint_t> childOffs(m_fm, callback(), src->children, src->numChildren);
  for ( unsigned int i=0; i<src->numChildren; ++i )
  {
    ObjectSharedPtr child(loadNode_nbf_12(childOffs[i]));
    if ( child )
    {
      DP_ASSERT( std::dynamic_pointer_cast<Node>(child) );
      dst->addChild(std::static_pointer_cast<Node>(child));
    }
  }
}

void DPBFLoader::readNode( NodeSharedPtr const& dst, const NBFNode * src )
{
  // annotation has been elevated to NBFObject with v61.2
  if (  m_nbfMajor < 0x3d
     || (m_nbfMajor==0x3d && m_nbfMinor < 0x02) )
  {
    if ( src->annotation.numChars )
    {
      dst->setAnnotation(mapString(src->annotation));
    }
  }

  // node hints were moved to object in later versions - but the bit
  // values are the same as in these versions, so this code is OK
  uint_t version = (m_nbfMajor << 8) & 0xFF00 | m_nbfMinor & 0xFF;

  if( version > 0x1202 && version < 0x3d05 )
  {
    const NBFNode_3d_04 * _src =
                        reinterpret_cast< const NBFNode_3d_04 * >( src );
    dst->setHints( (_src->userHints << 8) | _src->systemHints );
  }
}

void DPBFLoader::readFrustumCamera( FrustumCameraSharedPtr const& dst, uint_t offset )
{
  Offset_AutoPtr<NBFFrustumCamera> camPtr(m_fm, callback(), offset);

  readCamera( dst, camPtr );

  dst->setFarDistance(camPtr->farDist);
  dst->setNearDistance(camPtr->nearDist);
  dst->setWindowOffset(convert(camPtr->windowOffset));
  dst->setWindowSize(convert(camPtr->windowSize));
}

void DPBFLoader::readFrustumCamera_nbf_4c( FrustumCameraSharedPtr const& dst, uint_t offset )
{
  Offset_AutoPtr<NBFFrustumCamera_nbf_4c> camPtr(m_fm, callback(), offset);

  readCamera( dst, camPtr );

  dst->setFarDistance(camPtr->farDist);
  dst->setNearDistance(camPtr->nearDist);
  dst->setWindowOffset(convert(camPtr->windowOffset));
  dst->setWindowSize(convert(camPtr->windowSize));
  m_autoClipPlanes_nbf_4c = !!camPtr->isAutoClipPlanes;
}

void DPBFLoader::readCamera( CameraSharedPtr const& dst, const NBFCamera * src )
{
  readObject( dst, src );

  // headLights available?
  if ( src->numHeadLights )
  {
    Offset_AutoPtr<uint_t> lightOffs(m_fm, callback(), src->headLights, src->numHeadLights);
    for ( unsigned int i=0; i<src->numHeadLights; ++i )
    {
      LightSourceSharedPtr headlight((this->*m_pfnLoadLightSource)(lightOffs[i]));
      if ( headlight )
      {
        dst->addHeadLight(headlight);
      }
    }
  }

  Vec3f dir = convert(src->direction);
  dir.normalize();
  Vec3f up = convert(src->upVector);
  up.normalize();
  dst->setOrientation(dir, up);
  dst->setPosition(convert(src->position));
  dst->setFocusDistance(src->focusDist);
}

void DPBFLoader::readCamera_nbf_44( FrustumCameraSharedPtr const& dst, const NBFCamera_nbf_44 * src )
{
  readObject( dst, src );

  // headLights available?
  if ( src->numHeadLights )
  {
    Offset_AutoPtr<uint_t> lightOffs(m_fm, callback(), src->headLights, src->numHeadLights);
    for ( unsigned int i=0; i<src->numHeadLights; ++i )
    {
      LightSourceSharedPtr headlight((this->*m_pfnLoadLightSource)(lightOffs[i]));
      if ( headlight )
      {
        dst->addHeadLight(headlight);
      }
    }
  }

  Vec3f dir = convert(src->direction);
  dir.normalize();
  Vec3f up = convert(src->upVector);
  up.normalize();
  dst->setOrientation(dir, up);
  dst->setPosition(convert(src->position));
  dst->setFocusDistance(src->focusDist);
  dst->setWindowOffset(convert(src->windowOffset));
  dst->setWindowSize(convert(src->windowSize));
  dst->setFarDistance(src->farDist);
  dst->setNearDistance(src->nearDist);
  m_autoClipPlanes_nbf_4c = !!src->isAutoClipPlanes;
}

template <typename NBFTAIType>
inline ParameterGroupDataSharedPtr DPBFLoader::readTexAttribItem_nbf_54(Offset_AutoPtr<NBFTAIType>& nbfTAI)
{
  // undefined behavior if called for other objects!
  DP_ASSERT(nbfTAI->objectCode==DPBFCode::TEXTURE_ATTRIBUTE_ITEM);

  ParameterGroupDataSharedPtr textureData = getMaterialParameterGroup( "standardTextureParameters" );
  readObject( textureData, nbfTAI );

  TextureSharedPtr texture = readTexture( nbfTAI );
  readTexMatrix( textureData, nbfTAI );
  SamplerSharedPtr sampler = Sampler::create( texture );
  sampler->setBorderColor( convert<Vec4f>( nbfTAI->texBorderColor ) );
  sampler->setMagFilterMode( (TextureMagFilterMode)nbfTAI->magFilter );
  sampler->setMinFilterMode( (TextureMinFilterMode)nbfTAI->minFilter );
  sampler->setWrapModes( (TextureWrapMode)nbfTAI->texWrapS, (TextureWrapMode)nbfTAI->texWrapT, (TextureWrapMode)nbfTAI->texWrapR );

  readTexEnv( textureData, nbfTAI );
  readTexGenMode( textureData, nbfTAI );
  DP_VERIFY( textureData->setParameter( "sampler", sampler ) );
  DP_VERIFY( textureData->setParameter<bool>( "textureEnable", !!texture ) );

  return( textureData );
}

TextureSharedPtr DPBFLoader::readTexture( uint_t offset )
{
  string file;
  TextureHostSharedPtr texture = (this->*m_pfnLoadTextureHost)(offset, file);
  if ( texture && texture->getTextureTarget() == TextureTarget::UNSPECIFIED )
  {
    TextureTarget target = determineTextureTarget( texture );
    texture->convertToTextureTarget( target );
  }
  return( texture );
}

template <typename NBFTAIType>
TextureSharedPtr DPBFLoader::readTexture( const Offset_AutoPtr<NBFTAIType> & src )
{
  return( readTexture( src->texImg ) );
}

template <>
TextureSharedPtr DPBFLoader::readTexture( const Offset_AutoPtr<NBFTextureAttributeItem_nbf_4b> & src )
{
  string file;
  TextureHostSharedPtr texture = (this->*m_pfnLoadTextureHost)(src->texImg, file);
  if (texture && texture->getTextureTarget() == TextureTarget::UNSPECIFIED)
  {
    TextureTarget target = (TextureTarget)src->texTarget;
    if (target == TextureTarget::UNSPECIFIED)
    {
      target = determineTextureTarget(texture);
    }
    texture->convertToTextureTarget( target );
  }
  return( texture );
}

template <typename NBFTAIType>
void DPBFLoader::readTexEnv( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTAIType> & src )
{
  DP_VERIFY( dst->setParameter<dp::fx::EnumSpec::StorageType>( "envMode", src->texEnvMode ) );
}

template<>
inline void DPBFLoader::readTexEnv( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTextureAttributeItem_nbf_20> & src )
{
  DP_VERIFY( dst->setParameter<dp::fx::EnumSpec::StorageType>( "envMode", src->texEnvMode ) );
  DP_VERIFY( dst->setParameter( "envColor", convert<Vec4f>( src->texEnvColor ) ) );
}

template<>
inline void DPBFLoader::readTexEnv( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTextureAttributeItem_nbf_36> & src )
{
  DP_VERIFY( dst->setParameter<dp::fx::EnumSpec::StorageType>( "envMode", src->texEnvMode ) );
  DP_VERIFY( dst->setParameter( "envColor", convert<Vec4f>( src->texEnvColor ) ) );
  DP_VERIFY( dst->setParameter<char>( "envScale", src->texEnvScale ) );
}

template<>
inline void DPBFLoader::readTexEnv( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTextureAttributeItem_nbf_54> & src )
{
  DP_VERIFY( dst->setParameter<dp::fx::EnumSpec::StorageType>( "envMode", src->texEnvMode ) );
  DP_VERIFY( dst->setParameter( "envColor", convert<Vec4f>( src->texEnvColor ) ) );
  DP_VERIFY( dst->setParameter<char>( "envScale", src->texEnvScale ) );
}

template <typename NBFTAIType>
inline void DPBFLoader::readTexGenMode( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTAIType> & src )
{
  for ( unsigned int i=0 ; i<4 ; i++ )
  {
    DP_VERIFY( dst->setParameterArrayElement<dp::fx::EnumSpec::StorageType>( "genMode", i, src->texGenMode[i] + 1 ) );      // adjust for shift from -1,... to 0,...
    DP_VERIFY( dst->setParameterArrayElement<Vec4f>( "texGenPlane", i, convert<Vec4f>( src->texGenPlane[0][i] ) ) );
  }
}

template <typename NBFTAIType>
inline void DPBFLoader::readTexMatrix( ParameterGroupDataSharedPtr const& dst, const Offset_AutoPtr<NBFTAIType> & src )
{
  Trafo t;
  t.setOrientation( convert<Quatf>( src->trafo.orientation ) );
  t.setScaling( convert( src->trafo.scaling ) );
  t.setTranslation( convert( src->trafo.translation ) );
  DP_VERIFY( dst->setParameter( "textureMatrix", t.getMatrix() ) );
}
