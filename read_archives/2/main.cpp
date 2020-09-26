/*  Copyright (C) 2008 National Institute For Space Research (INPE) - Brazil.

    This file is part of the TerraLib - a Framework for building GIS enabled applications.

    TerraLib is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    TerraLib is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with TerraLib. See COPYING. If not, write to
    TerraLib Team at <terralib-team@terralib.org>.
 */

/*!
  \file terralib/qt/widgets/se/RasterSymbolizerWidget.cpp

  \brief A widget used to configure a Raster Symbolizer SE element.
*/

// TerraLib
#include "../../../common/STLUtils.h"
#include "../../../maptools/RasterContrast.h"
#include "../../../maptools/Utils.h"
#include "../../../raster.h"
#include "../../../se/Utils.h"
#include "../canvas/MapDisplay.h"
#include "../charts/ChartDisplay.h"
#include "../charts/ChartStyle.h"
#include "../charts/Histogram.h"
#include "../charts/HistogramChart.h"
#include "../charts/HistogramStyle.h"
#include "../rp/RasterHistogramWidget.h"
#include "../utils/HorizontalSliderWidget.h"
#include "../utils/ScopedCursor.h"
#include "RasterSymbolizerWidget.h"
#include "ui_RasterSymbolizerWidgetForm.h"

// Qt
#include <QLabel>
#include <QMessageBox>

// STL
#include <cassert>
#include <limits>

#define GAIN_CONSTANT_VALUE 0.1
#define OFFSET_CONSTANT_VALUE 10

te::qt::widgets::RasterSymbolizerWidget::RasterSymbolizerWidget(QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f),
    m_ui(new Ui::RasterSymbolizerWidgetForm),
    m_sliderWidget(nullptr),
    m_display(nullptr),
    m_symbolizer(nullptr),
    m_contrast(nullptr),
    m_contrastRed(nullptr),
    m_contrastGreen(nullptr),
    m_contrastBlue(nullptr),
    m_contrastMono(nullptr),
    m_scRed(nullptr),
    m_scGreen(nullptr),
    m_scBlue(nullptr),
    m_scMono(nullptr),
    m_cs(new te::se::ChannelSelection),
    m_setLocalSymbol(false)
{
  m_ui->setupUi(this);

  // add opacity scrool bar
  m_sliderWidget = new te::qt::widgets::HorizontalSliderWidget(m_ui->m_opacityWidget);
  m_sliderWidget->setTitle(tr("Opacity"));
  m_sliderWidget->setMinMaxValues(0, 100);
  m_sliderWidget->setDefaultValue(100);

  QGridLayout* layout = new QGridLayout(m_ui->m_opacityWidget);
  layout->setContentsMargins(0,0,0,0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_sliderWidget);

  // add histogram widget
  QGridLayout* histogramLayout = new QGridLayout(m_ui->m_histogramGroupBox);
  m_histogramWidget = new te::qt::widgets::RasterHistogramWidget(m_ui->m_histogramGroupBox);
  histogramLayout->addWidget(m_histogramWidget);
  histogramLayout->setContentsMargins(0, 0, 0, 0);

  m_histogramWidget->m_chartStyle->setAxisX("");
  m_histogramWidget->m_chartStyle->setAxisY("");
  m_histogramWidget->m_chartStyle->setGridChecked(false);
  m_histogramWidget->m_chartDisplay->enableAxis(QwtPlot::yLeft, false);
  m_histogramWidget->m_chartDisplay->insertLegend(nullptr);
  m_histogramWidget->m_chartDisplay->adjustDisplay();
  m_histogramWidget->m_chartDisplay->replot();

  m_ui->m_inMinLineEdit->setValidator(new QDoubleValidator(this));
  m_ui->m_inMaxLineEdit->setValidator(new QDoubleValidator(this));

  m_ui->m_typeComboBox->addItem(tr("Linear"), te::rp::Contrast::InputParameters::LinearContrastT);
  m_ui->m_typeComboBox->addItem(tr("Square Constrast"), te::rp::Contrast::InputParameters::SquareContrastT);
  m_ui->m_typeComboBox->addItem(tr("Square Root Constrast"), te::rp::Contrast::InputParameters::SquareRootContrastT);
  m_ui->m_typeComboBox->addItem(tr("Log Constrast"), te::rp::Contrast::InputParameters::LogContrastT);

  m_contrastMap[te::rp::Contrast::InputParameters::LinearContrastT] = te::map::RasterTransform::LINEAR_CONTRAST;
  m_contrastMap[te::rp::Contrast::InputParameters::SquareContrastT] = te::map::RasterTransform::SQUARE_CONTRAST;
  m_contrastMap[te::rp::Contrast::InputParameters::SquareRootContrastT] = te::map::RasterTransform::SQUARE_ROOT_CONTRAST;
  m_contrastMap[te::rp::Contrast::InputParameters::LogContrastT] = te::map::RasterTransform::LOG_CONTRAST;

  m_ui->m_allImageRadioButton->setChecked(true);

  //connect slots
  connect(m_sliderWidget, SIGNAL(sliderValueChanged(int)), this, SLOT(onOpacityChanged(int)));
  connect(m_sliderWidget, SIGNAL(sliderReleased()), this, SLOT(onSymbolizerChanged()));

  connect(m_ui->m_composeMRadioButton, SIGNAL(clicked()), this, SLOT(onMonoChannelSelectionClicked()));
  connect(m_ui->m_composeRRadioButton, SIGNAL(clicked()), this, SLOT(onRedChannelSelectionClicked()));
  connect(m_ui->m_composeGRadioButton, SIGNAL(clicked()), this, SLOT(onGreenChannelSelectionClicked()));
  connect(m_ui->m_composeBRadioButton, SIGNAL(clicked()), this, SLOT(onBlueChannelSelectionClicked()));
  connect(m_ui->m_composeCRadioButton, SIGNAL(clicked()), this, SLOT(onCompositionChannelSelectionClicked()));

  connect(m_ui->m_composeMComboBox, SIGNAL(activated(QString)), this, SLOT(onMonoChannelNameChanged(QString)));
  connect(m_ui->m_composeRComboBox, SIGNAL(activated(QString)), this, SLOT(onRedChannelNameChanged(QString)));
  connect(m_ui->m_composeGComboBox, SIGNAL(activated(QString)), this, SLOT(onGreenChannelNameChanged(QString)));
  connect(m_ui->m_composeBComboBox, SIGNAL(activated(QString)), this, SLOT(onBlueChannelNameChanged(QString)));

  connect(m_ui->m_contrastTypeComboBox, SIGNAL(activated(QString)), this, SLOT(onTypeConstratChanged(QString)));

  connect(m_ui->m_contrastMHorizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(onMonoGammaChanged(int)));
  connect(m_ui->m_contrastRHorizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(onRedGammaChanged(int)));
  connect(m_ui->m_contrastGHorizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(onGreenGammaChanged(int)));
  connect(m_ui->m_contrastBHorizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(onBlueGammaChanged(int)));

  connect(m_ui->m_contrastMHorizontalSlider, SIGNAL(sliderReleased()), this, SLOT(onSymbolizerChanged()));
  connect(m_ui->m_contrastRHorizontalSlider, SIGNAL(sliderReleased()), this, SLOT(onSymbolizerChanged()));
  connect(m_ui->m_contrastGHorizontalSlider, SIGNAL(sliderReleased()), this, SLOT(onSymbolizerChanged()));
  connect(m_ui->m_contrastBHorizontalSlider, SIGNAL(sliderReleased()), this, SLOT(onSymbolizerChanged()));

  connect(m_ui->m_gainPlusPushButton, SIGNAL(clicked()), this, SLOT(onIncreaseGain()));
  connect(m_ui->m_gainMinusPushButton, SIGNAL(clicked()), this, SLOT(onDecreaseGain()));
  connect(m_ui->m_gainResetPushButton, SIGNAL(clicked()), this, SLOT(onDefaultGain()));
  connect(m_ui->m_offsetPlusPushButton, SIGNAL(clicked()), this, SLOT(onIncreaseOffset()));
  connect(m_ui->m_offsetMinusPushButton, SIGNAL(clicked()), this, SLOT(onDecreaseOffset()));
  connect(m_ui->m_offsetResetPushButton, SIGNAL(clicked()), this, SLOT(onDefaultOffset()));

  connect(m_ui->m_dummyPushButton, SIGNAL(clicked()), this, SLOT(onDummyPushButtonClicked()));

  connect(m_ui->m_contrastGroupBox, SIGNAL(clicked()), this, SLOT(setContrastVisibility()));

  //contrast slots
  connect(m_ui->m_histogramToolButton, SIGNAL(clicked()), this, SLOT(onHistogramToolButtonClicked()));
  connect(m_ui->m_applyToolButton, SIGNAL(clicked()), this, SLOT(onApplyToolButtonClicked()));
  connect(m_ui->m_resetToolButton, SIGNAL(clicked()), this, SLOT(onResetToolButtonClicked()));

  m_ui->m_histogramToolButton->setIcon(QIcon::fromTheme("chart-bar"));
  m_ui->m_applyToolButton->setIcon(QIcon::fromTheme("check"));
  m_ui->m_resetToolButton->setIcon(QIcon::fromTheme("edit-undo"));
  

  //init interface
  initialize();
}

te::qt::widgets::RasterSymbolizerWidget::~RasterSymbolizerWidget()
{
  delete m_contrast;

  m_ceNames.clear();
}

void  te::qt::widgets::RasterSymbolizerWidget::setRasterSymbolizer(te::se::RasterSymbolizer* rs)
{
  assert(rs);

  m_setLocalSymbol = true;

  m_symbolizer = rs;

  if(m_symbolizer->getChannelSelection())
  {
    m_cs = m_symbolizer->getChannelSelection();

    m_scRed = m_cs->getRedChannel();
    m_scGreen = m_cs->getGreenChannel();
    m_scBlue = m_cs->getBlueChannel();
    m_scMono = m_cs->getGrayChannel();

    if(m_cs->getColorCompositionType() == te::se::RGB_COMPOSITION)
    {
      m_ui->m_composeCRadioButton->setChecked(true);

      onCompositionChannelSelectionClicked();
    }
    else if(m_cs->getColorCompositionType() == te::se::GRAY_COMPOSITION)
    {
      m_ui->m_composeMRadioButton->setChecked(true);

      onMonoChannelSelectionClicked();
    }
    else if(m_cs->getColorCompositionType() == te::se::RED_COMPOSITION)
    {
      m_ui->m_composeRRadioButton->setChecked(true);

      onRedChannelSelectionClicked();
    }
    else if(m_cs->getColorCompositionType() == te::se::GREEN_COMPOSITION)
    {
      m_ui->m_composeGRadioButton->setChecked(true);

      onGreenChannelSelectionClicked();
    }
    else if(m_cs->getColorCompositionType() == te::se::BLUE_COMPOSITION)
    {
      m_ui->m_composeBRadioButton->setChecked(true);

      onBlueChannelSelectionClicked();
    }

    m_contrastRed = nullptr;
    if(m_scRed && m_scRed->getContrastEnhancement())
    {
      m_contrastRed = m_scRed->getContrastEnhancement();
    }

    m_contrastGreen = nullptr;
    if(m_scGreen && m_scGreen->getContrastEnhancement())
    {
      m_contrastGreen = m_scGreen->getContrastEnhancement();
    }

    m_contrastBlue = nullptr;
    if(m_scBlue && m_scBlue->getContrastEnhancement())
    {
      m_contrastBlue = m_scBlue->getContrastEnhancement();
    }

    m_contrastMono = nullptr;
    if(m_scMono && m_scMono->getContrastEnhancement())
    {
      m_contrastMono = m_scMono->getContrastEnhancement();
    }
  }
  else
  {
    m_symbolizer->setChannelSelection(m_cs);
  }

  updateUi();

  m_setLocalSymbol = false;
}

void te::qt::widgets::RasterSymbolizerWidget::setRasterContrast(te::map::RasterContrast* contrast)
{
  delete m_contrast;

  m_contrast = new te::map::RasterContrast(*contrast);
}

te::map::RasterContrast* te::qt::widgets::RasterSymbolizerWidget::getRasterContrast()
{
  te::map::RasterContrast* rc = nullptr;
  
  if (m_contrast)
    rc = new te::map::RasterContrast(*m_contrast);

  return rc;
}

void te::qt::widgets::RasterSymbolizerWidget::setLayer(te::map::AbstractLayer* layer)
{
  if (!layer)
    return;

  m_layer = layer;

  std::unique_ptr<te::rst::Raster> raster(te::map::GetRaster(m_layer));

  if (raster->getBand(0)->getProperty()->getType() == te::dt::UCHAR_TYPE || raster->getBand(0)->getProperty()->getType() == te::dt::UINT16_TYPE ||
    raster->getBand(0)->getProperty()->getType() == te::dt::UINT32_TYPE || raster->getBand(0)->getProperty()->getType() == te::dt::UINT64_TYPE)
  {
    connect(m_histogramWidget, SIGNAL(minValueSelected(int, int)), this, SLOT(onMinValueSelected(int, int)));
    connect(m_histogramWidget, SIGNAL(maxValueSelected(int, int)), this, SLOT(onMaxValueSelected(int, int)));
  }
  else
  {
    connect(m_histogramWidget, SIGNAL(minValueSelected(double, int)), this, SLOT(onMinValueSelected(double, int)));
    connect(m_histogramWidget, SIGNAL(maxValueSelected(double, int)), this, SLOT(onMaxValueSelected(double, int)));
  }

  m_histogramWidget->clear();
  
  m_ui->m_inMinLineEdit->clear();
  m_ui->m_inMaxLineEdit->clear();

  m_ui->m_gainLineEdit->clear();
  m_ui->m_offset1LineEdit->clear();
  m_ui->m_offset2LineEdit->clear();

  delete m_contrast;
  m_contrast = nullptr;
}

void te::qt::widgets::RasterSymbolizerWidget::setBandProperty(std::vector<te::rst::BandProperty*> bp)
{
  QStringList bandNames;

  m_ui->m_bandComboBox->clear();

  for(size_t i = 0; i < bp.size(); ++i)
  {
    m_ui->m_bandComboBox->addItem(QString::number(i));

    // if the band property does not have the description information
    // we must used the index information.
    if(bp[i]->m_description.empty())
    {
      QString bandInfo;
      bandInfo.setNum(bp[i]->m_idx);

      bandNames.push_back(bandInfo);
    }
    else
    {
      bandNames.push_back(bp[i]->m_description.c_str());
    }

    if (i == 0)
    {
      if (bp[i]->m_noDataValue == std::numeric_limits<double>::max())
        m_ui->m_rasterDummyLineEdit->clear();
      else
        m_ui->m_rasterDummyLineEdit->setText(QString::number(bp[i]->m_noDataValue));
    }
  }

  m_ui->m_composeMComboBox->clear();
  m_ui->m_composeMComboBox->addItems(bandNames);

  m_ui->m_composeRComboBox->clear();
  m_ui->m_composeRComboBox->addItems(bandNames);

  m_ui->m_composeGComboBox->clear();
  m_ui->m_composeGComboBox->addItems(bandNames);

  m_ui->m_composeBComboBox->clear();
  m_ui->m_composeBComboBox->addItems(bandNames);
}

void te::qt::widgets::RasterSymbolizerWidget::setMapDisplay(te::qt::widgets::MapDisplay* display)
{
  m_display = display;
}

void te::qt::widgets::RasterSymbolizerWidget::initialize()
{
  //set the pixmaps
  m_ui->m_gainLabel->setPixmap(QIcon::fromTheme("gain").pixmap(16,16));
  m_ui->m_offSetLabel->setPixmap(QIcon::fromTheme("offset").pixmap(16,16));
  m_ui->m_gainPlusPushButton->setIcon(QIcon::fromTheme("list-add"));
  m_ui->m_gainPlusPushButton->setIconSize(QSize(16,16));
  m_ui->m_gainMinusPushButton->setIcon(QIcon::fromTheme("list-remove"));
  m_ui->m_gainMinusPushButton->setIconSize(QSize(16,16));
  m_ui->m_gainResetPushButton->setIcon(QIcon::fromTheme("edit-undo"));
  m_ui->m_gainResetPushButton->setIconSize(QSize(16,16));
  m_ui->m_offsetPlusPushButton->setIcon(QIcon::fromTheme("list-add"));
  m_ui->m_offsetPlusPushButton->setIconSize(QSize(16,16));
  m_ui->m_offsetMinusPushButton->setIcon(QIcon::fromTheme("list-remove"));
  m_ui->m_offsetMinusPushButton->setIconSize(QSize(16,16));
  m_ui->m_offsetResetPushButton->setIcon(QIcon::fromTheme("edit-undo"));
  m_ui->m_offsetResetPushButton->setIconSize(QSize(16,16));

  m_ui->m_dummyPushButton->setIcon(QIcon::fromTheme("check"));
  m_ui->m_dummyPushButton->setIconSize(QSize(16, 16));

  m_ui->m_composeMonoLabel->setPixmap(QIcon::fromTheme("bullet-black").pixmap(16,16));
  m_ui->m_composeRedLabel->setPixmap(QIcon::fromTheme("bullet-red").pixmap(16,16));
  m_ui->m_composeGreenLabel->setPixmap(QIcon::fromTheme("bullet-green").pixmap(16,16));
  m_ui->m_composeBlueLabel->setPixmap(QIcon::fromTheme("bullet-blue").pixmap(16,16));

  m_ui->m_composeCRadioButton->setIcon(QIcon::fromTheme("channels").pixmap(16,16));
  m_ui->m_composeRRadioButton->setIcon(QIcon::fromTheme("channel-red").pixmap(16,16));
  m_ui->m_composeGRadioButton->setIcon(QIcon::fromTheme("channel-green").pixmap(16,16));
  m_ui->m_composeBRadioButton->setIcon(QIcon::fromTheme("channel-blue").pixmap(16,16));
  m_ui->m_composeMRadioButton->setIcon(QIcon::fromTheme("channel-gray").pixmap(16,16));

  m_ui->m_rContrastLabel->setPixmap(QIcon::fromTheme("contrast-red").pixmap(16,16));
  m_ui->m_gContrastLabel->setPixmap(QIcon::fromTheme("contrast-green").pixmap(16,16));
  m_ui->m_bContrastLabel->setPixmap(QIcon::fromTheme("contrast-blue").pixmap(16,16));
  m_ui->m_mContrastLabel->setPixmap(QIcon::fromTheme("contrast-mono").pixmap(16,16));

   //set the contrast enhancement names
  m_ceNames.clear();

  m_ceNames.insert(std::map<te::se::ContrastEnhancement::ContrastEnhancementType, QString>::value_type
    (te::se::ContrastEnhancement::ENHANCEMENT_NORMALIZE, tr("Normalize")));
  m_ceNames.insert(std::map<te::se::ContrastEnhancement::ContrastEnhancementType, QString>::value_type
    (te::se::ContrastEnhancement::ENHANCEMENT_HISTOGRAM, tr("Histogram")));
  m_ceNames.insert(std::map<te::se::ContrastEnhancement::ContrastEnhancementType, QString>::value_type
    (te::se::ContrastEnhancement::ENHANCEMENT_NONE, tr("None")));

  std::map<te::se::ContrastEnhancement::ContrastEnhancementType, QString>::iterator it = m_ceNames.begin();

  while(it != m_ceNames.end())
  {
    m_ui->m_contrastTypeComboBox->addItem(it->second);

    ++it;
  }

  m_ui->m_contrastTypeComboBox->setCurrentIndex(m_ui->m_contrastTypeComboBox->findText(m_ceNames[te::se::ContrastEnhancement::ENHANCEMENT_NONE]));

  m_ui->m_contrastTypeComboBox->setVisible(false);
  m_ui->m_contrastTypeLabel->setVisible(false);

  // other values
  m_gainValue = 0.0;
  m_offsetValue = 0.0;
  m_gainOriginalValue = m_gainValue;
  m_offsetOriginalValue = m_offsetValue;

  m_ui->m_gainValueLabel->setText(QString::number(m_gainValue + 1.));
  m_ui->m_offSetValueLabel->setText(QString::number(m_offsetValue));
  m_ui->m_dummyLineEdit->setValidator(new QDoubleValidator(this));
}

void te::qt::widgets::RasterSymbolizerWidget::updateUi()
{
  if(m_symbolizer)
  {
    if(m_symbolizer->getOpacity())
    {
      double value = te::se::GetDouble(m_symbolizer->getOpacity()) * 100.;
      m_sliderWidget->setCurrentValue((int)value);
    }
    else
    {
      onOpacityChanged(m_sliderWidget->getValue());
    }

    if(m_symbolizer->getGain())
    {
      m_gainValue = te::se::GetDouble(m_symbolizer->getGain());
      m_gainOriginalValue = m_gainValue;
    }
    else
    {
      QString s;
      s.setNum(m_gainValue);
      m_symbolizer->setGain(new te::se::ParameterValue(s.toUtf8().data()));
    }

    m_ui->m_gainValueLabel->setText(QString::number(m_gainValue + 1.));

    if(m_symbolizer->getOffset())
    {
      m_offsetValue = te::se::GetDouble(m_symbolizer->getOffset());
      m_offsetOriginalValue = m_offsetValue;
    }
    else
    {
      QString s;
      s.setNum(m_offsetValue);
      m_symbolizer->setOffset(new te::se::ParameterValue(s.toUtf8().data()));
    }

    m_ui->m_offSetValueLabel->setText(QString::number(m_offsetValue));

    //no data value
    if (m_symbolizer->getNoDataValue())
    {
      double value = te::se::GetDouble(m_symbolizer->getNoDataValue());
      m_ui->m_dummyLineEdit->setText(QString::number(value));
    }
    else
    {
      m_ui->m_dummyLineEdit->clear();
    }

    //update channel selection
    m_ui->m_contrastGroupBox->setChecked(false);

    if(m_cs->getRedChannel())
    {
      te::se::SelectedChannel* channel = m_cs->getRedChannel();
      int index = atoi(channel->getSourceChannelName().c_str());
      m_ui->m_composeRComboBox->setCurrentIndex(index);

      if(channel->getContrastEnhancement())
      {
        te::se::ContrastEnhancement* ce = channel->getContrastEnhancement();
        double v = ce->getGammaValue() * 100.;
        m_ui->m_contrastRHorizontalSlider->setValue((int)v);
        m_ui->m_contrastTypeComboBox->setCurrentIndex(m_ui->m_contrastTypeComboBox->findText(m_ceNames[ce->getContrastEnhancementType()]));

        m_ui->m_contrastGroupBox->setChecked(true);
      }
      else
      {
        m_ui->m_contrastRHorizontalSlider->setValue(100.);
      }
    }

    if(m_cs->getGreenChannel())
    {
      te::se::SelectedChannel* channel = m_cs->getGreenChannel();
      int index = atoi(channel->getSourceChannelName().c_str());
      m_ui->m_composeGComboBox->setCurrentIndex(index);

      if(channel->getContrastEnhancement())
      {
        te::se::ContrastEnhancement* ce = channel->getContrastEnhancement();
        double v = ce->getGammaValue() * 100.;
        m_ui->m_contrastGHorizontalSlider->setValue((int)v);
        m_ui->m_contrastTypeComboBox->setCurrentIndex(m_ui->m_contrastTypeComboBox->findText(m_ceNames[ce->getContrastEnhancementType()]));

        m_ui->m_contrastGroupBox->setChecked(true);
      }
      else
      {
        m_ui->m_contrastGHorizontalSlider->setValue(100.);
      }
    }

    if(m_cs->getBlueChannel())
    {
      te::se::SelectedChannel* channel = m_cs->getBlueChannel();
      int index = atoi(channel->getSourceChannelName().c_str());
      m_ui->m_composeBComboBox->setCurrentIndex(index);

      if(channel->getContrastEnhancement())
      {
        te::se::ContrastEnhancement* ce = channel->getContrastEnhancement();
        double v = ce->getGammaValue() * 100.;
        m_ui->m_contrastBHorizontalSlider->setValue((int)v);
        m_ui->m_contrastTypeComboBox->setCurrentIndex(m_ui->m_contrastTypeComboBox->findText(m_ceNames[ce->getContrastEnhancementType()]));

        m_ui->m_contrastGroupBox->setChecked(true);
      }
      else
      {
        m_ui->m_contrastBHorizontalSlider->setValue(100.);
      }
    }

    if(m_cs->getGrayChannel())
    {
      te::se::SelectedChannel* channel = m_cs->getGrayChannel();
      int index = atoi(channel->getSourceChannelName().c_str());
      m_ui->m_composeMComboBox->setCurrentIndex(index);

      if(channel->getContrastEnhancement())
      {
        te::se::ContrastEnhancement* ce = channel->getContrastEnhancement();
        double v = ce->getGammaValue() * 100.;
        m_ui->m_contrastMHorizontalSlider->setValue((int)v);
        m_ui->m_contrastTypeComboBox->setCurrentIndex(m_ui->m_contrastTypeComboBox->findText(m_ceNames[ce->getContrastEnhancementType()]));

        m_ui->m_contrastGroupBox->setChecked(true);
      }
      else
      {
        m_ui->m_contrastMHorizontalSlider->setValue(100.);
      }
    }
  }

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::setComboBoxText(QComboBox* cb, std::string value)
{
  QString name = value.c_str();

  bool found = false;

  for(int i = 0; i < cb->count(); ++i)
  {
    if(cb->itemText(i) == name)
    {
      cb->setCurrentIndex(i);
      found = true;
      break;
    }
  }

  if(!found)
  {
    cb->addItem(name);
    cb->setCurrentIndex(cb->count() - 1);
  }
}

void te::qt::widgets::RasterSymbolizerWidget::onOpacityChanged(int value)
{
  if(m_symbolizer)
  {
    int opacity = value;
    double seOpacity = opacity / 100.;
    QString qStrOpacity;
    qStrOpacity.setNum(seOpacity);

    m_symbolizer->setOpacity(new te::se::ParameterValue(qStrOpacity.toUtf8().data()));
  }
}

void te::qt::widgets::RasterSymbolizerWidget::onMonoChannelSelectionClicked()
{
  m_ui->m_composeMComboBox->setEnabled(true);
  m_ui->m_composeRComboBox->setEnabled(false);
  m_ui->m_composeGComboBox->setEnabled(false);
  m_ui->m_composeBComboBox->setEnabled(false);

  if(m_scMono == nullptr)
  {
    m_scMono = new te::se::SelectedChannel();
    m_scMono->setSourceChannelName(std::to_string(m_ui->m_composeMComboBox->currentIndex()));
    m_cs->setGrayChannel(m_scMono);
  }

  m_cs->setColorCompositionType(te::se::GRAY_COMPOSITION);

  onSymbolizerChanged();

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::onRedChannelSelectionClicked()
{
  m_ui->m_composeMComboBox->setEnabled(false);
  m_ui->m_composeRComboBox->setEnabled(true);
  m_ui->m_composeGComboBox->setEnabled(false);
  m_ui->m_composeBComboBox->setEnabled(false);

  if(m_scRed == nullptr)
  {
    m_scRed = new te::se::SelectedChannel();
    m_scRed->setSourceChannelName(std::to_string(m_ui->m_composeRComboBox->currentIndex()));
    m_cs->setRedChannel(m_scRed);
  }

  m_cs->setColorCompositionType(te::se::RED_COMPOSITION);

  onSymbolizerChanged();

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::onGreenChannelSelectionClicked()
{
  m_ui->m_composeMComboBox->setEnabled(false);
  m_ui->m_composeRComboBox->setEnabled(false);
  m_ui->m_composeGComboBox->setEnabled(true);
  m_ui->m_composeBComboBox->setEnabled(false);

  if(m_scGreen== nullptr)
  {
    m_scGreen = new te::se::SelectedChannel();
    m_scGreen->setSourceChannelName(std::to_string(m_ui->m_composeGComboBox->currentIndex()));
    m_cs->setGreenChannel(m_scGreen);
  }

  m_cs->setColorCompositionType(te::se::GREEN_COMPOSITION);

  onSymbolizerChanged();

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::onBlueChannelSelectionClicked()
{
  m_ui->m_composeMComboBox->setEnabled(false);
  m_ui->m_composeRComboBox->setEnabled(false);
  m_ui->m_composeGComboBox->setEnabled(false);
  m_ui->m_composeBComboBox->setEnabled(true);

  if(m_scBlue== nullptr)
  {
    m_scBlue = new te::se::SelectedChannel();
    m_scBlue->setSourceChannelName(std::to_string(m_ui->m_composeBComboBox->currentIndex()));
    m_cs->setBlueChannel(m_scBlue);
  }

  m_cs->setColorCompositionType(te::se::BLUE_COMPOSITION);

  onSymbolizerChanged();

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::onCompositionChannelSelectionClicked()
{
  m_ui->m_composeMComboBox->setEnabled(false);
  m_ui->m_composeRComboBox->setEnabled(true);
  m_ui->m_composeGComboBox->setEnabled(true);
  m_ui->m_composeBComboBox->setEnabled(true);

  if(m_scRed == nullptr)
  {
    m_scRed = new te::se::SelectedChannel();
    m_scRed->setSourceChannelName(std::to_string(m_ui->m_composeRComboBox->currentIndex()));
    m_cs->setRedChannel(m_scRed);
  }

  if(m_scGreen== nullptr)
  {
    m_scGreen = new te::se::SelectedChannel();
    m_scGreen->setSourceChannelName(std::to_string(m_ui->m_composeGComboBox->currentIndex()));
    m_cs->setGreenChannel(m_scGreen);
  }

  if(m_scBlue== nullptr)
  {
    m_scBlue = new te::se::SelectedChannel();
    m_scBlue->setSourceChannelName(std::to_string(m_ui->m_composeBComboBox->currentIndex()));
    m_cs->setBlueChannel(m_scBlue);
  }

  m_cs->setColorCompositionType(te::se::RGB_COMPOSITION);

  onSymbolizerChanged();

  setContrastVisibility();
}

void te::qt::widgets::RasterSymbolizerWidget::onMonoChannelNameChanged(QString s)
{
  m_scMono->setSourceChannelName(std::to_string(m_ui->m_composeMComboBox->findText(s)));

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onRedChannelNameChanged(QString s)
{
  m_scRed->setSourceChannelName(std::to_string(m_ui->m_composeRComboBox->findText(s)));

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onGreenChannelNameChanged(QString s)
{
  m_scGreen->setSourceChannelName(std::to_string(m_ui->m_composeGComboBox->findText(s)));

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onBlueChannelNameChanged(QString s)
{
  m_scBlue->setSourceChannelName(std::to_string(m_ui->m_composeBComboBox->findText(s)));

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onTypeConstratChanged(QString s)
{
  if(m_contrastRed == nullptr)
  {
    m_contrastRed = new te::se::ContrastEnhancement();
    m_scRed->setContrastEnhancement(m_contrastRed);
  }

  if(m_contrastGreen == nullptr)
  {
    m_contrastGreen = new te::se::ContrastEnhancement();
    m_scGreen->setContrastEnhancement(m_contrastGreen);
  }

  if(m_contrastBlue == nullptr)
  {
    m_contrastBlue = new te::se::ContrastEnhancement();
    m_scBlue->setContrastEnhancement(m_contrastBlue);
  }

  if(m_contrastMono == nullptr)
  {
    m_contrastMono = new te::se::ContrastEnhancement();
    m_scMono->setContrastEnhancement(m_contrastMono);
  }

  std::map<te::se::ContrastEnhancement::ContrastEnhancementType, QString>::iterator it = m_ceNames.begin();

  while(it != m_ceNames.end())
  {
    if(it->second == s)
    {
      m_contrastRed->setContrastEnhancementType(it->first);
      m_contrastGreen->setContrastEnhancementType(it->first);
      m_contrastBlue->setContrastEnhancementType(it->first);
      m_contrastMono->setContrastEnhancementType(it->first);

      break;
    }

    ++it;
  }

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onMonoGammaChanged(int v)
{
  if(m_contrastMono == nullptr)
  {
    m_contrastMono = new te::se::ContrastEnhancement();
    m_scMono->setContrastEnhancement(m_contrastMono);
  }

  double value = double(v) / 100.;

  m_contrastMono->setGammaValue(value);
}

void te::qt::widgets::RasterSymbolizerWidget::onRedGammaChanged(int v)
{
  if(m_contrastRed == nullptr)
  {
    m_contrastRed = new te::se::ContrastEnhancement();
    m_scRed->setContrastEnhancement(m_contrastRed);
  }

  double value = double(v) / 100.;

  m_contrastRed->setGammaValue(value);
}

void te::qt::widgets::RasterSymbolizerWidget::onGreenGammaChanged(int v)
{
  if(m_contrastGreen == nullptr)
  {
    m_contrastGreen = new te::se::ContrastEnhancement();
    m_scGreen->setContrastEnhancement(m_contrastGreen);
  }

  double value = double(v) / 100.;

  m_contrastGreen->setGammaValue(value);
}

void te::qt::widgets::RasterSymbolizerWidget::onBlueGammaChanged(int v)
{
  if(m_contrastBlue == nullptr)
  {
    m_contrastBlue = new te::se::ContrastEnhancement();
    m_scBlue->setContrastEnhancement(m_contrastBlue);
  }

  double value = double(v) / 100.;

  m_contrastBlue->setGammaValue(value);
}

void te::qt::widgets::RasterSymbolizerWidget::onIncreaseGain()
{
  m_gainValue += GAIN_CONSTANT_VALUE;

  QString s;
  s.setNum(m_gainValue);

  if(m_symbolizer)
  {
    m_symbolizer->setGain(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_gainValueLabel->setText(QString::number(m_gainValue + 1.));
}

void te::qt::widgets::RasterSymbolizerWidget::onDecreaseGain()
{
  m_gainValue -= GAIN_CONSTANT_VALUE;

  QString s;
  s.setNum(m_gainValue);

  if(m_symbolizer)
  {
    m_symbolizer->setGain(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_gainValueLabel->setText(QString::number(m_gainValue + 1.));
}

void te::qt::widgets::RasterSymbolizerWidget::onDefaultGain()
{
  m_gainValue = m_gainOriginalValue;

  QString s;
  s.setNum(m_gainValue);

  if(m_symbolizer)
  {
    m_symbolizer->setGain(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_gainValueLabel->setText(QString::number(m_gainValue + 1.));
}

void te::qt::widgets::RasterSymbolizerWidget::onIncreaseOffset()
{
  m_offsetValue += OFFSET_CONSTANT_VALUE;

  QString s;
  s.setNum(m_offsetValue);
  
  if(m_symbolizer)
  {
    m_symbolizer->setOffset(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_offSetValueLabel->setText(QString::number(m_offsetValue));
}

void te::qt::widgets::RasterSymbolizerWidget::onDecreaseOffset()
{
  m_offsetValue -= OFFSET_CONSTANT_VALUE;

  QString s;
  s.setNum(m_offsetValue);
  
  if(m_symbolizer)
  {
    m_symbolizer->setOffset(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_offSetValueLabel->setText(QString::number(m_offsetValue));
}

void te::qt::widgets::RasterSymbolizerWidget::onDefaultOffset()
{
  m_offsetValue = m_offsetOriginalValue;

  QString s;
  s.setNum(m_offsetValue);

  if(m_symbolizer)
  {
    m_symbolizer->setOffset(new te::se::ParameterValue(s.toUtf8().data()));

    onSymbolizerChanged();
  }

  m_ui->m_offSetValueLabel->setText(QString::number(m_offsetValue));
}

void te::qt::widgets::RasterSymbolizerWidget::onSymbolizerChanged()
{
  if (m_setLocalSymbol)
    return;

  emit symbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onDummyPushButtonClicked()
{
 if (m_ui->m_dummyLineEdit->text().isEmpty())
  {
    if (m_symbolizer)
      m_symbolizer->setNoDataValue(nullptr);
  }
  else
  {
    if (m_symbolizer)
      m_symbolizer->setNoDataValue(new te::se::ParameterValue(m_ui->m_dummyLineEdit->text().toUtf8().data()));
  }

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::setContrastVisibility()
{
  if(m_ui->m_contrastGroupBox->isChecked() == false)
  {
    m_ui->m_contrastRHorizontalSlider->setValue(100);
    m_ui->m_contrastGHorizontalSlider->setValue(100);
    m_ui->m_contrastBHorizontalSlider->setValue(100);
    m_ui->m_contrastMHorizontalSlider->setValue(100);

    if (m_contrastMono)
    {
      m_scMono->setContrastEnhancement(nullptr);
      m_contrastMono = nullptr;
    }

    if (m_contrastRed)
    {
      m_scRed->setContrastEnhancement(nullptr);
      m_contrastRed = nullptr;
    }

    if (m_contrastGreen)
    {
      m_scGreen->setContrastEnhancement(nullptr);
      m_contrastGreen = nullptr;
    }

    if (m_contrastBlue)
    {
      m_scBlue->setContrastEnhancement(nullptr);
      m_contrastBlue = nullptr;
    }

    onSymbolizerChanged();

    return;
  }

  m_ui->m_contrastRHorizontalSlider->setEnabled(false);
  m_ui->m_contrastGHorizontalSlider->setEnabled(false);
  m_ui->m_contrastBHorizontalSlider->setEnabled(false);
  m_ui->m_contrastMHorizontalSlider->setEnabled(false);

  if(m_ui->m_composeMRadioButton->isChecked())
  {
    m_ui->m_contrastMHorizontalSlider->setEnabled(true);
  }

  if(m_ui->m_composeRRadioButton->isChecked())
  {
    m_ui->m_contrastRHorizontalSlider->setEnabled(true);
  }

  if(m_ui->m_composeGRadioButton->isChecked())
  {
    m_ui->m_contrastGHorizontalSlider->setEnabled(true);
  }

  if(m_ui->m_composeBRadioButton->isChecked())
  {
    m_ui->m_contrastBHorizontalSlider->setEnabled(true);
  }

  if(m_ui->m_composeCRadioButton->isChecked())
  {
    m_ui->m_contrastRHorizontalSlider->setEnabled(true);
    m_ui->m_contrastGHorizontalSlider->setEnabled(true);
    m_ui->m_contrastBHorizontalSlider->setEnabled(true);
  }
}

void te::qt::widgets::RasterSymbolizerWidget::onHistogramToolButtonClicked()
{
  int bandIdx = m_ui->m_bandComboBox->currentText().toInt();

  std::unique_ptr<te::rst::Raster> raster(te::map::GetRaster(m_layer));

  te::gm::Envelope* visibleAreaRasterProj = nullptr;

  if (m_display)
  {
    visibleAreaRasterProj = new te::gm::Envelope(m_display->getExtent());
    visibleAreaRasterProj->transform(m_display->getSRID(), m_layer->getSRID());

    std::map<std::string, std::string> info;
    info["FORCE_MEM_DRIVER"] = "TRUE";

    if(m_ui->m_visibleAreaRadioButton->isChecked())
    {
      raster.reset(raster->trim(visibleAreaRasterProj, info));
    }
    else
    {
      te::gm::Envelope* env = new te::gm::Envelope(m_layer->getExtent());

      std::unique_ptr<te::rst::Raster> rasterExt(te::map::GetRaster(m_layer));

      raster.reset(rasterExt->trim(env, info));

      double resx = visibleAreaRasterProj->getWidth() / static_cast<double>(m_display->getWidth());
      double resy = visibleAreaRasterProj->getHeight() / static_cast<double>(m_display->getHeight());

      double rx = resx / raster->getResolutionX();
      double ry = resy / raster->getResolutionY();

      double factor = std::min(rx, ry);
      factor = std::max(factor, 1.0);

      std::size_t level = static_cast<std::size_t>(std::log(factor) / std::log(2.0));
      std::size_t numberOfLevels = raster->getMultiResLevelsCount();

      te::rst::Raster* overview = nullptr;

      if (level != 0 && numberOfLevels != 0) // need overview? has overview?
      {
        if (numberOfLevels >= level)
          overview = raster->getMultiResLevel(static_cast<unsigned int>(level - 1));
        else
          overview = raster->getMultiResLevel(static_cast<unsigned int>(numberOfLevels - 1));

        //adjust raster overview parameters

        if (raster->getNumberOfBands() == overview->getNumberOfBands())
        {
          for (std::size_t t = 0; t < raster->getNumberOfBands(); ++t)
          {
            te::rst::BandProperty* bp = raster->getBand(t)->getProperty();
            te::rst::BandProperty* bpOverView = overview->getBand(t)->getProperty();
            bpOverView->m_noDataValue = bp->m_noDataValue;
            bpOverView->m_min = bp->m_min;
            bpOverView->m_max = bp->m_max;
          }
        }

        overview->getGrid()->setSRID(raster->getSRID());

        raster.reset(overview);
      }

      delete env;
    }
  }
  
  te::qt::widgets::ScopedCursor c(Qt::WaitCursor);

  m_histogramWidget->setInputRaster(raster.get());

  double min = 0.;
  double max = 0.;

  te::rst::BandProperty* bp = raster->getBand(bandIdx)->getProperty();

  if (bp->m_min != std::numeric_limits<double>::max() &&
    bp->m_max != std::numeric_limits<double>::max())
  {
    min = bp->m_min;
    max = bp->m_max;
  }
  else
  {
    te::rst::GetDataTypeRanges(bp->getType(), min, max);
  }

  m_histogramWidget->updateMinimumValueLine(min, false);
  m_histogramWidget->updateMaximumValueLine(max, false);

  m_histogramWidget->updateMinimumValueLabel(tr("Suggested Minimum"));
  m_histogramWidget->updateMaximumValueLabel(tr("Suggested Maximum"));

  m_ui->m_inMinLineEdit->setText(QString::number(min));
  m_ui->m_inMaxLineEdit->setText(QString::number(max));

  if (m_contrast)
  {
    std::vector<double> gain, offset1, offset2, min, max;
    m_contrast->getValues(gain, offset1, offset2, min, max);

    m_ui->m_gainLineEdit->setText(QString::number(gain[bandIdx]));
    m_ui->m_offset1LineEdit->setText(QString::number(offset1[bandIdx]));
    m_ui->m_offset2LineEdit->setText(QString::number(offset2[bandIdx]));


    // only set minimum and maximum values if values was defined by user
    bool defaultValues = false;

    if (gain[bandIdx] == 1. && offset1[bandIdx] == 0. && offset2[bandIdx] == 0.)
      defaultValues = true;

    if (!defaultValues)
    {
      m_ui->m_inMinLineEdit->setText(QString::number(min[bandIdx]));
      m_ui->m_inMaxLineEdit->setText(QString::number(max[bandIdx]));

      m_histogramWidget->updateMinimumValueLine(min[bandIdx], false);
      m_histogramWidget->updateMaximumValueLine(max[bandIdx], false);

      m_histogramWidget->updateMinimumValueLabel(tr("Acquired Minimum"));
      m_histogramWidget->updateMaximumValueLabel(tr("Acquired Maximum"));
    }
  }

  m_histogramWidget->drawHistogram(bandIdx);
}

void te::qt::widgets::RasterSymbolizerWidget::onMinValueSelected(int value, int band)
{
  m_ui->m_inMinLineEdit->setText(QString::number(value));

  m_histogramWidget->updateMinimumValueLine(value, true);
}

void te::qt::widgets::RasterSymbolizerWidget::onMaxValueSelected(int value, int band)
{
  m_ui->m_inMaxLineEdit->setText(QString::number(value));

  m_histogramWidget->updateMaximumValueLine(value, true);
}

void te::qt::widgets::RasterSymbolizerWidget::onMinValueSelected(double value, int band)
{
  m_ui->m_inMinLineEdit->setText(QString::number(value));

  m_histogramWidget->updateMinimumValueLine(value, true);
}

void te::qt::widgets::RasterSymbolizerWidget::onMaxValueSelected(double value, int band)
{
  m_ui->m_inMaxLineEdit->setText(QString::number(value));

  m_histogramWidget->updateMaximumValueLine(value, true);
}

void te::qt::widgets::RasterSymbolizerWidget::onResetToolButtonClicked()
{
  delete m_contrast;
  m_contrast = nullptr;

  if (m_layer)
    m_layer->setRasterContrast(nullptr);

  m_ui->m_gainLineEdit->clear();
  m_ui->m_offset1LineEdit->clear();
  m_ui->m_offset2LineEdit->clear();

  m_ui->m_inMinLineEdit->clear();
  m_ui->m_inMaxLineEdit->clear();

  m_histogramWidget->clear();

  emit contrastChanged(false);

  onSymbolizerChanged();
}

void te::qt::widgets::RasterSymbolizerWidget::onApplyToolButtonClicked()
{
  int type = m_ui->m_typeComboBox->currentData().toInt();
  int typeMapped = m_contrastMap[(te::rp::Contrast::InputParameters::ContrastType)type];

  if (!m_contrast)
  {
    m_contrast = new te::map::RasterContrast((te::map::RasterTransform::ContrastType)typeMapped, m_ui->m_bandComboBox->count());
  }

  if (m_ui->m_inMinLineEdit->text().isEmpty() || m_ui->m_inMaxLineEdit->text().isEmpty())
  {
    QMessageBox::warning(this, tr("Warning"), tr("Input values not defined."));
    return;
  }
  
  double minValue = m_ui->m_inMinLineEdit->text().toDouble();
  double maxValue = m_ui->m_inMaxLineEdit->text().toDouble();

  if (minValue >= maxValue)
  {
    QMessageBox::warning(this, tr("Warning"), tr("Invalid input values."));
    return;
  }

  double gain, offset1, offset2;

  if (te::rp::Contrast::getGainAndOffset((te::rp::Contrast::InputParameters::ContrastType)type, minValue, maxValue, 0., 255., gain, offset1, offset2))
  {
    int bandIdx = m_ui->m_bandComboBox->currentText().toInt();

    m_contrast->setValues(gain, offset1, offset2, minValue, maxValue, bandIdx);

    m_ui->m_gainLineEdit->setText(QString::number(gain));
    m_ui->m_offset1LineEdit->setText(QString::number(offset1));
    m_ui->m_offset2LineEdit->setText(QString::number(offset2));

    emit contrastChanged(false);

    onSymbolizerChanged();

    emit mapRefresh();
  }
  else
  {
    QMessageBox::warning(this, tr("Warning"), tr("Internal error."));
    return;
  }
}
