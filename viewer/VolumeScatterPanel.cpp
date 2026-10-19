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

    anisotropyBox = addUnitSpin(form, "Anisotropy (g)", 0.6, -0.99, 0.99, 0.05);
    emissionStrengthBox = addUnitSpin(form, "Emission strength", 0., 0., 3., 0.001);
    emissionStrengthBox->setDecimals(4);
    outer->addWidget(group);

    auto hook = [this]() { emitSettingsChanged(); };
    connect(enabledBox, &QCheckBox::toggled, this, hook);
    connect(maxBouncesBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emitSettingsChanged(); });
    for (auto *box : {anisotropyBox, emissionStrengthBox}) {
      connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
              this, [this](double) { emitSettingsChanged(); });
    }
  }

  void VolumeScatterPanel::setSettings(const VolumeScatterSettings &settings)
  {
    suppressSignals = true;
    enabledBox->setChecked(settings.enabled);
    maxBouncesBox->setValue(settings.maxVolumeBounces);
    anisotropyBox->setValue(settings.medium.anisotropy);
    emissionStrengthBox->setValue(settings.medium.emissionStrength);
    suppressSignals = false;
  }

  VolumeScatterSettings VolumeScatterPanel::getSettings()
  {
    VolumeScatterSettings settings;
    settings.enabled = enabledBox->isChecked();
    settings.maxVolumeBounces = maxBouncesBox->value();
    settings.medium.anisotropy = float(anisotropyBox->value());
    settings.medium.emissionStrength = float(emissionStrengthBox->value());
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
