// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if HS_USE_MULTI_SCATTERING

#include "hayStack/NanoVDBVolume.h"

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
    QDoubleSpinBox *scatteringAlbedoBox = nullptr;
    QDoubleSpinBox *densityBox = nullptr;
    QDoubleSpinBox *densityThresholdBox = nullptr;
    QDoubleSpinBox *emissionStrengthBox = nullptr;
    QDoubleSpinBox *blackbodyIntensityBox = nullptr;
    QDoubleSpinBox *temperatureBox = nullptr;
    QDoubleSpinBox *scatterColor[3]{};
    QDoubleSpinBox *absorptionColor[3]{};
    QDoubleSpinBox *emissionColor[3]{};
    QDoubleSpinBox *blackbodyTint[3]{};

    QWidget *paramsWidget = nullptr;
    bool suppressSignals = false;

    static QDoubleSpinBox *addUnitSpin(QFormLayout *layout,
                                       const char *label,
                                       double value,
                                       double minVal,
                                       double maxVal,
                                       double step);
    static void addColorSpinRow(QFormLayout *layout,
                                const char *label,
                                QDoubleSpinBox *rgb[3],
                                const mini::common::vec3f &value);
    static mini::common::vec3f readColor(QDoubleSpinBox *rgb[3]);
  };

}

#endif
