// SPDX-FileCopyrightText: Copyright (c) 2023-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayMaker/TextureLibrary.h"
#include "hayMaker/AnariDeviceRenderer.h"

namespace hm {

  TextureLibrary::TextureLibrary(AnariDeviceRenderer *renderer)
    : renderer(renderer)
  {}

  anari::Sampler
  TextureLibrary::getOrCreate(mini::Texture::SP miniTex)
  {
    auto it = alreadyCreated.find(miniTex);
    if (it != alreadyCreated.end()) return it->second;

    auto bnTex = create(miniTex);
    alreadyCreated[miniTex] = bnTex;
    return bnTex;
  }
  
  anari::Sampler
  TextureLibrary::create(mini::Texture::SP miniTex)
  {
    if (!miniTex) return 0;
    
    // auto device = device;
    std::string filterMode;
    switch (miniTex->filterMode) {
    case mini::Texture::FILTER_BILINEAR:
      /*! default filter mode - bilinear */
      filterMode = "linear";
      break;
    case mini::Texture::FILTER_NEAREST:
      /*! explicitly request nearest-filtering */
      filterMode = "nearest";
      break;
    default:
      std::cout << "warning: unsupported mini::Texture filter mode #"
                << (int)miniTex->filterMode << std::endl;
      return 0;
    }

    std::string wrapMode   = "mirrorRepeat";
    // BNTextureData texData = bnTextureData2DCreate(global->model,this->slot,
    //                                               texelFormat,
    //                                               miniTex->size.x,miniTex->size.y,
    //                                               miniTex->data.data());

    anari::Array2D image;
    switch (miniTex->format) {
    case mini::Texture::FLOAT4:
      image = anariNewArray2D(renderer->anari.device,
                              (const void *)miniTex->data.data(),
                              nullptr,nullptr,ANARI_FLOAT32_VEC4,
                              (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    case mini::Texture::FLOAT1:
      image = anariNewArray2D(renderer->anari.device,
                              (const void **)miniTex->data.data(),
                              0,0,ANARI_FLOAT32,
                              (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    case mini::Texture::RGBA_UINT8:
      image = anariNewArray2D(renderer->anari.device,
                              (const void *)miniTex->data.data(),
                              0,0,ANARI_UFIXED8_VEC4,
                              (size_t)miniTex->size.x,(size_t)miniTex->size.y);
      break;
    default:
      std::cout << "warning: unsupported mini::Texture format #"
                << (int)miniTex->format << std::endl;
      return 0;
    }
    anari::commitParameters(renderer->anari.device,image);

    anari::Sampler sampler
      = anari::newObject<anari::Sampler>(renderer->anari.device,"image2D");
    assert(sampler);
    // anari::setParameter(device,sampler,"inAttribute","attribute0");
    anari::setParameter(renderer->anari.device,sampler,"wrapMode1",wrapMode);
    anari::setParameter(renderer->anari.device,sampler,"wrapMode2",wrapMode);
    anari::setParameter(renderer->anari.device,sampler,"filterMode",filterMode);
    anari::setParameter(renderer->anari.device,sampler,"image",image);
    anari::commitParameters(renderer->anari.device,sampler);
    return sampler;
  }  
}
