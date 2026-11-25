// SPDX-FileCopyrightText: Copyright (c) 2025++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hayStack/HayStack.h"

namespace hs {

  /*! common base class for all of haystack's volume types
      (structured, tamr, nanovdb, ...). Like everything else in
      haystack, an instance of this stores only *ONE RANK'S PART* of
      what may - across all ranks - be a logically larger volume */
  struct Volume {
    typedef std::shared_ptr<Volume> SP;

    Volume(const std::string &xfName)
      : xfName(xfName)
    {}
    virtual ~Volume() = default;

    /*! bounds of this rank's part of the volume */
    virtual box3f getBounds() const = 0;
    /*! range of scalar values in this rank's part of the volume */
    virtual range1f getValueRange() const = 0;

    /*! name of the transfer function to use for this volume */
    std::string xfName;
  };

}
