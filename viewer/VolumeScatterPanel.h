// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if HS_USE_MULTI_SCATTERING

#include "hayStack/VolumeScatter.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QWidget>

namespace hs {

  struct VolumeScatterPanel : public QWidget
  {
    Q_OBJECT

  public:
    VolumeScatterPanel(QWidget *parent = nullptr);

    void setSettings(const VolumeScatterSettings &settings);
    VolumeScatterSettings getSettings();

  signals:
    void settingsChanged(const hs::VolumeScatterSettings &settings);

  private slots:
    void emitSettingsChanged();

  private:
    QCheckBox *enabledBox = nullptr;
    QSpinBox *maxBouncesBox = nullptr;
    QDoubleSpinBox *anisotropyBox = nullptr;
    QDoubleSpinBox *emissionStrengthBox = nullptr;

    bool suppressSignals = false;

    static QDoubleSpinBox *addUnitSpin(QFormLayout *layout,
                                       const char *label,
                                       double value,
                                       double minVal,
                                       double maxVal,
                                       double step);
  };

}

#endif
