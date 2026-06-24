// SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

#if HS_USE_MULTI_SCATTERING

#include "viewer/VolumeScatterPanel.h"

#include <QVBoxLayout>

namespace hs {

  QDoubleSpinBox *VolumeScatterPanel::addUnitSpin(QFormLayout *layout,
                                                  const char *label,
                                                  double value,
                                                  double minVal,
                                                  double maxVal,
                                                  double step)
  {
    auto *box = new QDoubleSpinBox;
    box->setDecimals(3);
    box->setRange(minVal, maxVal);
    box->setSingleStep(step);
    box->setValue(value);
    layout->addRow(label, box);
    return box;
  }

  void VolumeScatterPanel::addColorSpinRow(QFormLayout *layout,
                                           const char *label,
                                           QDoubleSpinBox *rgb[3],
                                           const mini::common::vec3f &value)
  {
    auto *row = new QWidget;
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    const char *channels[] = {"R", "G", "B"};
    const float vals[] = {value.x, value.y, value.z};
    for (int i = 0; i < 3; ++i) {
      auto *box = new QDoubleSpinBox;
      box->setPrefix(QString(channels[i]) + " ");
      box->setDecimals(3);
      box->setRange(0., 10.);
      box->setSingleStep(0.05);
      box->setValue(vals[i]);
      rgb[i] = box;
      rowLayout->addWidget(box);
    }
    layout->addRow(label, row);
  }

  mini::common::vec3f VolumeScatterPanel::readColor(QDoubleSpinBox *rgb[3])
  {
    return {float(rgb[0]->value()),
            float(rgb[1]->value()),
            float(rgb[2]->value())};
  }

  VolumeScatterPanel::VolumeScatterPanel(QWidget *parent)
    : QWidget(parent)
  {
    auto *outer = new QVBoxLayout(this);
    auto *group = new QGroupBox("Volume multi-scattering");
    auto *form = new QFormLayout(group);

    enabledBox = new QCheckBox("Enable in-volume multi-scattering");
    enabledBox->setChecked(true);
    form->addRow(enabledBox);

    maxBouncesBox = new QSpinBox;
    maxBouncesBox->setRange(1, 64);
    maxBouncesBox->setValue(8);
    form->addRow("Max volume bounces", maxBouncesBox);

    paramsWidget = new QWidget;
    auto *paramsForm = new QFormLayout(paramsWidget);

    anisotropyBox = addUnitSpin(paramsForm, "Anisotropy (g)", 0.6, -0.99, 0.99, 0.05);
    scatteringAlbedoBox = addUnitSpin(paramsForm, "Scattering albedo", 0.9, 0., 1., 0.05);
    addColorSpinRow(paramsForm, "Scatter color", scatterColor, {0.8f, 0.8f, 0.8f});
    addColorSpinRow(paramsForm, "Absorption color", absorptionColor, {0.f, 0.f, 0.f});
    densityBox = addUnitSpin(paramsForm, "Density", 1., 0., 100., 0.1);
    densityThresholdBox = addUnitSpin(paramsForm, "Density threshold", 0., 0., 1., 0.01);
    emissionStrengthBox = addUnitSpin(paramsForm, "Emission strength", 0., 0., 100., 0.1);
    addColorSpinRow(paramsForm, "Emission color", emissionColor, {1.f, 1.f, 1.f});
    blackbodyIntensityBox = addUnitSpin(paramsForm, "Blackbody intensity", 0., 0., 100., 0.1);
    addColorSpinRow(paramsForm, "Blackbody tint", blackbodyTint, {1.f, 1.f, 1.f});
    temperatureBox = addUnitSpin(paramsForm, "Temperature (K)", 0., 0., 40000., 100.);

    form->addRow(paramsWidget);
    outer->addWidget(group);

    auto hook = [this]() { emitSettingsChanged(); };
    connect(enabledBox, &QCheckBox::toggled, this, hook);
    connect(maxBouncesBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emitSettingsChanged(); });
    for (auto *box : {anisotropyBox, scatteringAlbedoBox, densityBox,
                      densityThresholdBox, emissionStrengthBox,
                      blackbodyIntensityBox, temperatureBox}) {
      connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
              this, [this](double) { emitSettingsChanged(); });
    }
    for (int i = 0; i < 3; ++i) {
      for (auto *rgb : {scatterColor, absorptionColor, emissionColor, blackbodyTint}) {
        connect(rgb[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { emitSettingsChanged(); });
      }
    }

    connect(enabledBox, &QCheckBox::toggled, paramsWidget, &QWidget::setEnabled);
  }

  void VolumeScatterPanel::setSettings(const VolumeScatterSettings &settings)
  {
    suppressSignals = true;
    enabledBox->setChecked(settings.enabled);
    maxBouncesBox->setValue(settings.maxVolumeBounces);
    anisotropyBox->setValue(settings.medium.anisotropy);
    scatteringAlbedoBox->setValue(settings.medium.scatteringAlbedo);
    densityBox->setValue(settings.medium.density);
    densityThresholdBox->setValue(settings.medium.densityThreshold);
    emissionStrengthBox->setValue(settings.medium.emissionStrength);
    blackbodyIntensityBox->setValue(settings.medium.blackbodyIntensity);
    temperatureBox->setValue(settings.medium.temperature);

    const mini::common::vec3f colorValues[] = {
      settings.medium.scatterColor,
      settings.medium.absorptionColor,
      settings.medium.emissionColor,
      settings.medium.blackbodyTint,
    };
    QDoubleSpinBox *colorSpin[][3] = {
      {scatterColor[0], scatterColor[1], scatterColor[2]},
      {absorptionColor[0], absorptionColor[1], absorptionColor[2]},
      {emissionColor[0], emissionColor[1], emissionColor[2]},
      {blackbodyTint[0], blackbodyTint[1], blackbodyTint[2]},
    };
    for (int c = 0; c < 4; ++c) {
      for (int i = 0; i < 3; ++i)
        colorSpin[c][i]->setValue((&colorValues[c].x)[i]);
    }
    paramsWidget->setEnabled(settings.enabled);
    suppressSignals = false;
  }

  VolumeScatterSettings VolumeScatterPanel::getSettings()
  {
    VolumeScatterSettings settings;
    settings.enabled = enabledBox->isChecked();
    settings.maxVolumeBounces = maxBouncesBox->value();
    settings.medium.anisotropy = float(anisotropyBox->value());
    settings.medium.scatteringAlbedo = float(scatteringAlbedoBox->value());
    settings.medium.scatterColor = readColor(scatterColor);
    settings.medium.absorptionColor = readColor(absorptionColor);
    settings.medium.density = float(densityBox->value());
    settings.medium.densityThreshold = float(densityThresholdBox->value());
    settings.medium.emissionStrength = float(emissionStrengthBox->value());
    settings.medium.emissionColor = readColor(emissionColor);
    settings.medium.blackbodyIntensity = float(blackbodyIntensityBox->value());
    settings.medium.blackbodyTint = readColor(blackbodyTint);
    settings.medium.temperature = float(temperatureBox->value());
    return settings;
  }

  void VolumeScatterPanel::emitSettingsChanged()
  {
    if (suppressSignals)
      return;
    emit settingsChanged(getSettings());
  }

}

#endif
