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
  \file terralib/qt/widgets/se/ColorMapWidget.cpp

  \brief A dialog used to build a Color Map element.
*/

// TerraLib

#include "../../../color/ColorBar.h"
#include "../../../common/STLUtils.h"
#include "../../../dataaccess/dataset/DataSet.h"
#include "../../../dataaccess/dataset/DataSetType.h"
#include "../../../dataaccess/utils/Utils.h"
#include "../../../datatype.h"
#include "../../../maptools/DataSetLayer.h"
#include "../../../maptools/GroupingAlgorithms.h"
#include "../../../maptools/RasterLayer.h"
#include "../../../fe/Literal.h"
#include "../../../fe/Utils.h"
#include "../../../raster.h"
#include "../../../raster/RasterSummary.h"
#include "../../../raster/RasterSummaryManager.h"
#include "../../../se/ColorMap.h"
#include "../../../se/Categorize.h"
#include "../../../se/Interpolate.h"
#include "../../../se/InterpolationPoint.h"
#include "../../../se/MapItem.h"
#include "../../../se/ParameterValue.h"
#include "../../../se/RasterSymbolizer.h"
#include "../../../se/Recode.h"
#include "../../../se/Rule.h"
#include "../../../se/Utils.h"
#include "../../widgets/colorbar/ColorBar.h"
#include "../../widgets/colorbar/ColorCatalogWidget.h"
#include "../../widgets/utils/ScopedCursor.h"
#include "ColorMapWidget.h"
#include "ui_ColorMapWidgetForm.h"

// Qt
#include <QColorDialog>
#include <QMessageBox>
#include <QPainter>
#include <QValidator>
#include <QFileDialog>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>

// STL
#include <cassert>

Q_DECLARE_METATYPE(te::map::AbstractLayerPtr)

te::qt::widgets::ColorMapWidget::ColorMapWidget(QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f),
    m_ui(new Ui::ColorMapWidgetForm),
    m_cm(nullptr),
    m_cb(nullptr),
    m_raster(nullptr)
{
  m_ui->setupUi(this);

  QGridLayout* l = new QGridLayout(m_ui->m_colorBarWidget);
  l->setContentsMargins(0,0,0,0);
  m_colorBar = new  te::qt::widgets::ColorCatalogWidget(m_ui->m_colorBarWidget);
  l->addWidget(m_colorBar);

  m_ui->m_minValueLineEdit->setValidator(new QDoubleValidator(this));
  m_ui->m_maxValueLineEdit->setValidator(new QDoubleValidator(this));
  
  initialize();

  // Signals & slots
  connect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onApplyPushButtonClicked()));
  connect(m_ui->m_bandComboBox, SIGNAL(activated(QString)), this, SLOT(onBandSelected(QString)));
  connect(m_ui->m_applyPushButton, SIGNAL(clicked()), this, SLOT(onApplyPushButtonClicked()));
  connect(m_ui->m_tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onTableWidgetItemDoubleClicked(QTableWidgetItem*)));
  connect(m_ui->m_importPushButton, SIGNAL(clicked()), this, SLOT(onImportPushButtonClicked()));
  connect(m_ui->m_transformComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onTransformComboBoxCurrentIndexChanged(int)));
  connect(m_ui->m_loadLegendPushButton, SIGNAL(clicked()), this, SLOT(onLoadPushButtonClicked()));
  connect(m_ui->m_saveLegendPushButton, SIGNAL(clicked()), this, SLOT(onSavePushButtonClicked()));
}

te::qt::widgets::ColorMapWidget::~ColorMapWidget()
{
  delete m_cb;

  delete m_cm;
}

void te::qt::widgets::ColorMapWidget::setRaster(te::rst::Raster* r)
{
  assert(r);

  m_raster = r;

  int nBands = static_cast<int>(m_raster->getNumberOfBands());

  m_ui->m_bandComboBox->clear();

  for(int i = 0; i < nBands; ++i)
  {
    QString strBand;
    strBand.setNum(i);

    m_ui->m_bandComboBox->addItem(strBand);
  }

  if(nBands > 0)
    onBandSelected(m_ui->m_bandComboBox->itemText(0));
}

void te::qt::widgets::ColorMapWidget::setColorMap(te::se::ColorMap* cm) 
{
  assert(cm);

  m_cm = cm->clone();

  updateUi(true);
}

te::se::ColorMap* te::qt::widgets::ColorMapWidget::getColorMap()
{
  if (m_ui->m_tableWidget->rowCount() < 1)
    return nullptr;

  int index = m_ui->m_transformComboBox->currentIndex();

  int type = m_ui->m_transformComboBox->itemData(index).toInt();
  
  if(type == te::se::CATEGORIZE_TRANSFORMATION)
  {
    buildCategorizationMap();
  }
  else if(type == te::se::INTERPOLATE_TRANSFORMATION)
  {
    buildInterpolationMap();
  }
  else if(type == te::se::RECODE_TRANSFORMATION)
  {
    buildRecodingMap();
  }

  return m_cm->clone();
}

std::string te::qt::widgets::ColorMapWidget::getCurrentBand()
{
  if(m_ui->m_bandComboBox->count() != 0)
  {
    return m_ui->m_bandComboBox->currentText().toUtf8().data();
  }

  return "";
}

Ui::ColorMapWidgetForm * te::qt::widgets::ColorMapWidget::getForm()
{
  return m_ui.get();
}

void te::qt::widgets::ColorMapWidget::initialize()
{
  m_colorBar->getColorBar()->setHeight(20);
  m_colorBar->getColorBar()->setScaleVisible(false);

  m_ui->m_transformComboBox->clear();

  m_ui->m_transformComboBox->addItem(tr("Categorize"), te::se::CATEGORIZE_TRANSFORMATION);
  m_ui->m_transformComboBox->addItem(tr("Interpolate"), te::se::INTERPOLATE_TRANSFORMATION);
  m_ui->m_transformComboBox->addItem(tr("Recode"), te::se::RECODE_TRANSFORMATION);

  m_ui->m_typeComboBox->addItem(tr("Equal Steps"), te::se::EQUAL_STEP_SLICE);
  m_ui->m_typeComboBox->addItem(tr("Unique Value"), te::se::UNIQUE_VALUE_SLICE);
}

void te::qt::widgets::ColorMapWidget::updateUi(bool loadColorBar)
{
  m_ui->m_tableWidget->setRowCount(0);

  if(!m_cm)
  {
    return;
  }

  te::color::ColorBar* cb = nullptr;

  if(m_cm->getCategorize())
  {
    updateTableHeader(te::se::CATEGORIZE_TRANSFORMATION);

    for(int i = 0; i < m_ui->m_transformComboBox->count(); ++i)
    {
      if(m_ui->m_transformComboBox->itemData(i).toInt() == te::se::CATEGORIZE_TRANSFORMATION)
        m_ui->m_transformComboBox->setCurrentIndex(i);
    }

    std::vector<te::se::ParameterValue*> t = m_cm->getCategorize()->getThresholds();
    std::vector<te::se::ParameterValue*> tV = m_cm->getCategorize()->getThresholdValues();

    m_ui->m_slicesSpinBox->setValue(static_cast<int>(tV.size() - 2));

    m_ui->m_tableWidget->setRowCount(static_cast<int>(tV.size() - 2));

    te::color::RGBAColor initColor(te::se::GetString(tV[1]).c_str());
    te::color::RGBAColor endColor(te::se::GetString(tV[tV.size() - 2]).c_str());

    if(loadColorBar)
      cb = new te::color::ColorBar(initColor, endColor, 256);

    int count = 0;

    for(size_t i = 1; i < tV.size() - 1; ++i)
    {
      QColor color;
      std::string lowerLimit = "";
      std::string upperLimit = "";

      if(i == 0)
      {
        lowerLimit = "...";
        upperLimit = te::se::GetString(t[i]);
        color.setNamedColor(te::se::GetString(tV[i]).c_str());
      }
      else if(i == tV.size() - 1)
      {
        lowerLimit = te::se::GetString(t[i - 1]);
        upperLimit = "...";
        color.setNamedColor(te::se::GetString(tV[i]).c_str());
      }
      else
      {
        lowerLimit = te::se::GetString(t[i - 1]);
        upperLimit = te::se::GetString(t[i]);
        color.setNamedColor(te::se::GetString(tV[i]).c_str());
      }

      if(loadColorBar)
      {
        if(count != 0 && count != static_cast<int>(tV.size()) - 2)
        {
          double pos = (1. / (tV.size() - 2)) * count;

          te::color::RGBAColor color(te::se::GetString(tV[i]).c_str());

          cb->addColor(color, pos);
        }
      }
              
      ++count;

      //color
      QPixmap pix(24, 24);
      QPainter p(&pix);
      
      p.fillRect(0,0,24, 24, color);
      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(pix, "");
      item->setFlags(Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(i - 1), 0, item);

      //value From
      std::string rangeLower = lowerLimit;
      QTableWidgetItem* itemRangeLower = new QTableWidgetItem();
      itemRangeLower->setText(rangeLower.c_str());
      itemRangeLower->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(i - 1), 1, itemRangeLower);

      //value To
      std::string rangeUpper = upperLimit;
      QTableWidgetItem* itemRangeUpper = new QTableWidgetItem();
      itemRangeUpper->setText(rangeUpper.c_str());
      itemRangeUpper->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);

      m_ui->m_tableWidget->setItem(static_cast<int>(i - 1), 2, itemRangeUpper);
    }
  }
  else if(m_cm->getInterpolate())
  {
    updateTableHeader(te::se::INTERPOLATE_TRANSFORMATION);

    for(int i = 0; i < m_ui->m_transformComboBox->count(); ++i)
    {
      if(m_ui->m_transformComboBox->itemData(i).toInt() == te::se::INTERPOLATE_TRANSFORMATION)
        m_ui->m_transformComboBox->setCurrentIndex(i);
    }

    std::vector<te::se::InterpolationPoint*> ip = m_cm->getInterpolate()->getInterpolationPoints();

    m_ui->m_slicesSpinBox->setValue(static_cast<int>(ip.size() - 1));

    m_ui->m_tableWidget->setRowCount(static_cast<int>(ip.size() - 1));

    te::color::RGBAColor initColor(te::se::GetString(ip[0]->getValue()).c_str());
    te::color::RGBAColor endColor(te::se::GetString(ip[ip.size() - 1]->getValue()).c_str());

    if(loadColorBar)
      cb = new te::color::ColorBar(initColor, endColor, 256);

    int count = 0;

    for(size_t i = 0; i < ip.size() - 1; ++i)
    {
      QColor color1;
      QColor color2;
      QString valStrBegin;
      QString valStrEnd;

      te::se::InterpolationPoint* ipItem = ip[i];
      te::se::InterpolationPoint* ipItem2 = ip[i + 1];
            
      color1.setNamedColor(te::se::GetString(ipItem->getValue()).c_str());
      color2.setNamedColor(te::se::GetString(ipItem2->getValue()).c_str());

      valStrBegin.setNum(ipItem->getData());
      valStrEnd.setNum(ipItem2->getData());

      QString valStr;
      valStr.append(valStrBegin);
      valStr.append(" - ");
      valStr.append(valStrEnd);

      if(loadColorBar)
      {
        if(count != 0 && count != static_cast<int>(ip.size()) - 1)
        {
          double pos = (1. / (ip.size() - 1)) * count;

          te::color::RGBAColor color(te::se::GetString(ipItem->getValue()).c_str());

          cb->addColor(color, pos);
        }
      }

      ++count;

    //color
      QPixmap pix(24, 24);
      QPainter p(&pix);
      
      p.fillRect(0,0,12, 24, color1);
      p.fillRect(12,0,12, 24, color2);
      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(icon, "");
      //item->setBackgroundColor(color);
      item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 0, item);

      //value lower
      QTableWidgetItem* itemRangeLower = new QTableWidgetItem();
      itemRangeLower->setText(valStrBegin);
      itemRangeLower->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 1, itemRangeLower);

      //value upper
      QTableWidgetItem* itemRangeUpper = new QTableWidgetItem();
      itemRangeUpper->setText(valStrEnd);
      itemRangeUpper->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 2, itemRangeUpper);
    }
  }
  else if (m_cm->getRecode())
  {
    updateTableHeader(te::se::RECODE_TRANSFORMATION);

    for (int i = 0; i < m_ui->m_transformComboBox->count(); ++i)
    {
      if (m_ui->m_transformComboBox->itemData(i).toInt() == te::se::RECODE_TRANSFORMATION)
        m_ui->m_transformComboBox->setCurrentIndex(i);
    }

    if (loadColorBar)
      cb = new te::color::ColorBar;

    te::se::Recode* rec = m_cm->getRecode();

    std::vector<te::se::MapItem*> items = rec->getMapItems();

    m_ui->m_tableWidget->setRowCount(static_cast<int>(items.size()));

    int count = 0;

    for (std::size_t i = 0; i < items.size(); ++i)
    {
      std::string title = items[i]->getTitle();
      double data = items[i]->getData();
      te::se::ParameterValue* value = items[i]->getValue();

      if (loadColorBar)
      {
        te::color::RGBAColor color(te::se::GetString(value).c_str());

        if (count == 0)
          cb->addColor(color, 0);
        else if (count == static_cast<int>(items.size())-1)
          cb->addColor(color, 1);
        else
        {
          double pos = (1. / (items.size() - 2)) * count;

          cb->addColor(color, pos);
        }
      }

      ++count;

      QColor color;
      color.setNamedColor(te::se::GetString(value).c_str());

      //color
      QPixmap pix(24, 24);
      QPainter p(&pix);

      p.fillRect(0, 0, 24, 24, color);
      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(pix, "");
      item->setFlags(Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 0, item);

      // Data
      QTableWidgetItem* itemData = new QTableWidgetItem();
      itemData->setText(QString::number(data));
      itemData->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);// | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 1, itemData);

      // Title
      QTableWidgetItem* itemTitle = new QTableWidgetItem();
      itemTitle->setText(title.c_str());
      itemTitle->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(i), 2, itemTitle);
    }
  }

  if(cb)
  {
    disconnect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onApplyPushButtonClicked()));

    te::qt::widgets::colorbar::ColorBar* cbW = m_colorBar->getColorBar();
    cbW->setColorBar(cb);

    connect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onApplyPushButtonClicked()));
  }

  m_ui->m_tableWidget->resizeColumnsToContents();
#if (QT_VERSION >= 0x050000)
  m_ui->m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
  m_ui->m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif
}

void te::qt::widgets::ColorMapWidget::buildCategorizationMap()
{
  te::se::Categorize* c = new te::se::Categorize();

  c->setFallbackValue("#000000");
  c->setLookupValue(new te::se::ParameterValue("Rasterdata"));

  QColor cWhite(Qt::white);
  std::string colorWhiteStr = cWhite.name().toUtf8().data();

  //added dummy color for values < than min values...
  c->addValue(new te::se::ParameterValue(colorWhiteStr));

  for(int i = 0; i < m_ui->m_tableWidget->rowCount(); ++i)
  {
    QColor color = QColor::fromRgb(m_ui->m_tableWidget->item(i, 0)->icon().pixmap(24, 24).toImage().pixel(1,1));

    std::string rangeStr = m_ui->m_tableWidget->item(i, 1)->text().toUtf8().data();
    std::string colorStr = color.name().toUtf8().data();

    c->addThreshold(new te::se::ParameterValue(rangeStr));
    c->addValue(new te::se::ParameterValue(colorStr));

    if(i == m_ui->m_tableWidget->rowCount() - 1)
    {
      rangeStr = m_ui->m_tableWidget->item(i, 2)->text().toUtf8().data();
      c->addThreshold(new te::se::ParameterValue(rangeStr));
    }
  }

  //added dummy color for values > than max values...
  c->addValue(new te::se::ParameterValue(colorWhiteStr));

  c->setThresholdsBelongTo(te::se::Categorize::SUCCEEDING);

  if(m_cm)
  {
    m_cm->setCategorize(c);
    m_cm->setInterpolate(nullptr);
    m_cm->setRecode(nullptr);
  }
}

void te::qt::widgets::ColorMapWidget::buildInterpolationMap()
{
  te::se::Interpolate* interpolate = new te::se::Interpolate();

  interpolate->setFallbackValue("#000000");
  interpolate->setLookupValue(new te::se::ParameterValue("Rasterdata"));
  interpolate->setMethodType(te::se::Interpolate::COLOR);

  for(int i = 0; i < m_ui->m_tableWidget->rowCount(); ++i)
  {
    QColor color = QColor::fromRgb(m_ui->m_tableWidget->item(i, 0)->icon().pixmap(24, 24).toImage().pixel(1,1));

    if(i == m_ui->m_tableWidget->rowCount() - 1)
    {
      {
        QString rangeStr = m_ui->m_tableWidget->item(i, 1)->text();
        std::string colorStr = color.name().toUtf8().data();

        te::se::InterpolationPoint* ip = new te::se::InterpolationPoint();

        ip->setData(rangeStr.toDouble());
        ip->setValue(new te::se::ParameterValue(colorStr));

        interpolate->add(ip);
      }

      {
        color = QColor::fromRgb(m_ui->m_tableWidget->item(i, 0)->icon().pixmap(24, 24).toImage().pixel(22,1));

        QString rangeStr = m_ui->m_tableWidget->item(i, 2)->text();
        std::string colorStr = color.name().toUtf8().data();

        te::se::InterpolationPoint* ip = new te::se::InterpolationPoint();

        ip->setData(rangeStr.toDouble());
        ip->setValue(new te::se::ParameterValue(colorStr));

        interpolate->add(ip);
      }

    }
    else
    {
      QString rangeStr = m_ui->m_tableWidget->item(i, 1)->text();
      std::string colorStr = color.name().toUtf8().data();

      te::se::InterpolationPoint* ip = new te::se::InterpolationPoint();

      ip->setData(rangeStr.toDouble());
      ip->setValue(new te::se::ParameterValue(colorStr));

      interpolate->add(ip);
    }
  }

  if(m_cm)
  {
    m_cm->setInterpolate(interpolate);
    m_cm->setCategorize(nullptr);
    m_cm->setRecode(nullptr);
  }
}

void te::qt::widgets::ColorMapWidget::buildRecodingMap()
{
  te::se::Recode* r = new te::se::Recode();

  r->setFallbackValue("#000000");
  r->setLookupValue(new te::se::ParameterValue("Rasterdata"));

  for (int i = 0; i < m_ui->m_tableWidget->rowCount(); ++i)
  {
    QColor color = QColor::fromRgb(m_ui->m_tableWidget->item(i, 0)->icon().pixmap(24, 24).toImage().pixel(1, 1));

    double data = m_ui->m_tableWidget->item(i, 1)->text().toDouble();
    std::string colorStr = color.name().toUtf8().data();
    std::string title = m_ui->m_tableWidget->item(i, 2)->text().toUtf8().data();

    te::se::MapItem* m = new te::se::MapItem();
    m->setData(data);
    m->setValue(new te::se::ParameterValue(colorStr));
    m->setTitle(title);

    r->add(m);
  }

  if (m_cm)
  {
    m_cm->setCategorize(nullptr);
    m_cm->setInterpolate(nullptr);
    m_cm->setRecode(r);
  }
}

void te::qt::widgets::ColorMapWidget::onApplyPushButtonClicked()
{
  delete m_cb;

  m_cb = m_colorBar->getColorBar()->getColorBar();

  int sliceValue = m_ui->m_slicesSpinBox->value();

  std::vector<te::color::RGBAColor> colorVec;

  int index = m_ui->m_transformComboBox->currentIndex();

  int transType = m_ui->m_transformComboBox->itemData(index).toInt();

  updateTableHeader((te::se::ColorMapTransformationType)transType);

  if (transType == te::se::CATEGORIZE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(sliceValue);
  }
  else if (transType == te::se::INTERPOLATE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(sliceValue + 1);
  }
  else if (transType == te::se::RECODE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(static_cast<int>(getRecodeValues().size()));
  }

  std::vector<te::se::Rule*> legVec;

  std::vector<double> vec;
  vec.push_back(m_ui->m_minValueLineEdit->text().toDouble());
  vec.push_back(m_ui->m_maxValueLineEdit->text().toDouble());

  te::qt::widgets::ScopedCursor cursor(Qt::WaitCursor);

  int sliceType = m_ui->m_typeComboBox->itemData(index).toInt();

  if (sliceType == te::se::EQUAL_STEP_SLICE)
  {
    te::map::GroupingByEqualSteps("raster", vec.begin(), vec.end(), sliceValue, legVec, m_ui->m_precisionSpinBox->value());
  }
  else if (sliceType == te::se::UNIQUE_VALUE_SLICE)
  {
    std::vector<std::string> values = getValues();
    std::vector<std::string> valuesAux;

    for (std::size_t i = 0; i < values.size(); ++i)
    {
      double val = boost::lexical_cast<double>(values[i]);

      if (val >= m_ui->m_minValueLineEdit->text().toDouble() && val <= m_ui->m_maxValueLineEdit->text().toDouble())
      {
        valuesAux.push_back(values[i]);
      }
    }

    te::map::GroupingByUniqueValues("raster", valuesAux, te::dt::STRING_TYPE, legVec, m_ui->m_precisionSpinBox->value());
  }

  m_ui->m_tableWidget->setRowCount(static_cast<int>(legVec.size()));

  for(std::size_t t = 0; t < legVec.size(); ++t)
  {
    if (transType == te::se::CATEGORIZE_TRANSFORMATION)
    {
      //color
      QColor color(colorVec[t].getRed(), colorVec[t].getGreen(), colorVec[t].getBlue(), colorVec[t].getAlpha());

      QPixmap pix(24, 24);
      QPainter p(&pix);
      
      p.fillRect(0,0,24, 24, color);
      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(icon, "");
      item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 0, item);
    }
    else if (transType == te::se::RECODE_TRANSFORMATION)
    {
      QColor color(colorVec[t].getRed(), colorVec[t].getGreen(), colorVec[t].getBlue(), colorVec[t].getAlpha());

      QPixmap pix(24, 24);
      QPainter p(&pix);

      p.fillRect(0, 0, 24, 24, color);
      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(icon, "");
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 0, item);
    }
    else if (transType == te::se::INTERPOLATE_TRANSFORMATION)
    {
      QColor color1(colorVec[t].getRed(), colorVec[t].getGreen(), colorVec[t].getBlue(), colorVec[t].getAlpha());
      QColor color2(colorVec[t + 1].getRed(), colorVec[t + 1].getGreen(), colorVec[t + 1].getBlue(), colorVec[t + 1].getAlpha());

      QPixmap pix(24, 24);
      QPainter p(&pix);
      
      p.fillRect(0,0,12, 24, color1);
      p.fillRect(12,0,12, 24, color2);

      p.setBrush(Qt::transparent);
      p.setPen(Qt::black);
      p.drawRect(0, 0, 23, 23);

      QIcon icon(pix);

      QTableWidgetItem* item = new QTableWidgetItem(icon, "");
      item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 0, item);
    }

    if (transType == te::se::RECODE_TRANSFORMATION)
    {
      //value lower
      std::string value = "";
      std::string title = *legVec[t]->getName();

      te::fe::GetFilterUniqueValue(legVec[t]->getFilter(), value);

      QTableWidgetItem* itemValue = new QTableWidgetItem();
      itemValue->setText(value.c_str());
      itemValue->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 1, itemValue);

      QTableWidgetItem* itemTitle = new QTableWidgetItem();
      itemTitle->setText(title.c_str());
      itemTitle->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 2, itemTitle);
    }
    else
    {
      
      std::string rangeStrLower = "";
      std::string rangeStrUpper = "";

      te::fe::GetFilterStepValues(legVec[t]->getFilter(), rangeStrLower, rangeStrUpper);

      //value lower
      QTableWidgetItem* itemRangeLower = new QTableWidgetItem();
      itemRangeLower->setText(rangeStrLower.c_str());
      itemRangeLower->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 1, itemRangeLower);

      //value upper
      QTableWidgetItem* itemRangeUpper = new QTableWidgetItem();
      itemRangeUpper->setText(rangeStrUpper.c_str());
      itemRangeUpper->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(static_cast<int>(t), 2, itemRangeUpper);
    }
  }

  m_ui->m_tableWidget->resizeColumnsToContents();
#if (QT_VERSION >= 0x050000)
  m_ui->m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
  m_ui->m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

  emit applyPushButtonClicked();
}

void te::qt::widgets::ColorMapWidget::onBandSelected(QString value)
{
  if(value.isEmpty())
  {
    return;
  }

  const te::rst::RasterSummary* rsMin = te::rst::RasterSummaryManager::getInstance().get(m_raster, te::rst::SUMMARY_MIN);
  const te::rst::RasterSummary* rsMax = te::rst::RasterSummaryManager::getInstance().get(m_raster, te::rst::SUMMARY_MAX);
  const std::complex<double>* cmin = rsMin->at(value.toInt()).m_minVal;
  const std::complex<double>* cmax = rsMax->at(value.toInt()).m_maxVal;
  double min = cmin->real();
  double max = cmax->real();

  QString strMin;
  strMin.setNum(min);
  m_ui->m_minValueLineEdit->setText(strMin);

  QString strMax;
  strMax.setNum(max);
  m_ui->m_maxValueLineEdit->setText(strMax);

  te::rst::Band* band = m_raster->getBand(value.toInt());
  te::rst::ColorInterp interp = band->getProperty()->m_colorInterp;
  
  int pos = m_ui->m_transformComboBox->findText("Recode");

  if (interp != te::rst::PaletteIdxCInt)
  {
    if (pos >= 0)
    {
      m_ui->m_transformComboBox->removeItem(pos);
    }
  }
  else
  {
    if (pos < 0)
    {
      m_ui->m_transformComboBox->addItem(tr("Recode"), te::se::RECODE_TRANSFORMATION);
    }    
  }

  emit applyPushButtonClicked();
}

void te::qt::widgets::ColorMapWidget::onTableWidgetItemDoubleClicked(QTableWidgetItem* item)
{
  int curCol = m_ui->m_tableWidget->currentColumn();

  int index = m_ui->m_transformComboBox->currentIndex();

  int type = m_ui->m_transformComboBox->itemData(index).toInt();

  if(curCol == 0)
  {
    if (type == te::se::CATEGORIZE_TRANSFORMATION || type == te::se::RECODE_TRANSFORMATION)
    {
      QColor bgColor = QColor::fromRgb(item->icon().pixmap(24, 24).toImage().pixel(1,1));

      QColor newBgColor = QColorDialog::getColor(bgColor, m_ui->m_tableWidget);

      if(newBgColor.isValid())
        bgColor = newBgColor;
    
      QPixmap pix(24, 24);
      pix.fill(bgColor);
      QIcon icon(pix);

      item->setIcon(icon);
    }
    else if(type == te::se::INTERPOLATE_TRANSFORMATION)
    {
      QMessageBox::information(this, tr("Classification"), 
        tr("Set the colors for the min and max values of this range. Also necessary to change the colors equivalents at another level to maintain consistency."));

      QColor color1 = QColor::fromRgb(item->icon().pixmap(24, 24).toImage().pixel(1,1));

      QColor newBgColor = QColorDialog::getColor(color1, m_ui->m_tableWidget);

      if(newBgColor.isValid())
        color1 = newBgColor;

      QColor color2 = QColor::fromRgb(item->icon().pixmap(24, 24).toImage().pixel(22,1));

      newBgColor = QColorDialog::getColor(color2, m_ui->m_tableWidget);

      if(newBgColor.isValid())
        color2 = newBgColor;

      QPixmap pix(24, 24);
      QPainter p(&pix);
      
      p.fillRect(0,0,12, 24, color1);
      p.fillRect(12,0,12, 24, color2);

      QIcon icon(pix);

      item->setIcon(icon);
    }
  }

  emit applyPushButtonClicked();
}

te::se::ColorMap* getLayerColorMap(te::map::AbstractLayerPtr layer)
{
  te::se::RasterSymbolizer* symb = nullptr;

  if(layer->getType() == "DATASETLAYER")
  {
    te::map::DataSetLayer* l = dynamic_cast<te::map::DataSetLayer*>(layer.get());

    if(l)
    {
      symb = te::se::GetRasterSymbolizer(l->getStyle());
    }
  }
  else if(layer->getType() == "RASTERLAYER")
  {
    te::map::RasterLayer* l = dynamic_cast<te::map::RasterLayer*>(layer.get());

    if(l)
    {
      symb = te::se::GetRasterSymbolizer(l->getStyle());
    }
  }

  if(symb)
  {
    if(symb->getColorMap())
    {
      return symb->getColorMap();
    }
  }

  return nullptr;
}

te::rst::Raster* getLayerRaster(te::map::AbstractLayerPtr layer)
{
  if(layer->getType() == "DATASETLAYER")
  {
    te::map::DataSetLayer* l = dynamic_cast<te::map::DataSetLayer*>(layer.get());

    if(l)
    {
      std::unique_ptr<te::da::DataSet> ds = layer->getData();

      if(ds.get())
      {
        std::size_t rpos = te::da::GetFirstPropertyPos(ds.get(), te::dt::RASTER_TYPE);
        return ds->getRaster(rpos).release();
      }
    }
  }
  else if(layer->getType() == "RASTERLAYER")
  {
    te::map::RasterLayer* l = dynamic_cast<te::map::RasterLayer*>(layer.get());

    if(l)
    {
      return l->getRaster();
    }
  }

  return nullptr;
}

void te::qt::widgets::ColorMapWidget::setLayers(te::map::AbstractLayerPtr selectedLayer, std::vector<te::map::AbstractLayerPtr> allLayers)
{
  for(std::size_t i = 0; i < allLayers.size(); ++i)
  {
    std::unique_ptr<te::da::DataSetType> dt(allLayers[i]->getSchema());

    if(dt->hasRaster() && getLayerColorMap(allLayers[i]) && allLayers[i]->getId() != selectedLayer->getId())
    {
      m_ui->m_layersComboBox->addItem(allLayers[i]->getTitle().c_str(), QVariant::fromValue(allLayers[i]));
    }
  }
}

void te::qt::widgets::ColorMapWidget::onImportPushButtonClicked()
{
  if(m_ui->m_layersComboBox->currentText() == "")
  {
    QMessageBox::warning(this, tr("Grouping"), tr("There are no other layers with Grouping!"));
    return;
  }
  
  QVariant varLayer = m_ui->m_layersComboBox->itemData(m_ui->m_layersComboBox->currentIndex(), Qt::UserRole);
  te::map::AbstractLayerPtr layer = varLayer.value<te::map::AbstractLayerPtr>();

  te::se::ColorMap* cm = getLayerColorMap(layer);

  setColorMap(cm);

  updateUi(true);

  emit applyPushButtonClicked();
}

void te::qt::widgets::ColorMapWidget::onLoadPushButtonClicked()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open..."),
    QString(), tr("LEG (*.leg *.LEG);;"), nullptr, QFileDialog::DontConfirmOverwrite);

  if (fileName.isEmpty())
    return;

  m_ui->m_tableWidget->setRowCount(0);

  boost::property_tree::ptree pt;

  try
  {
    boost::property_tree::json_parser::read_json(fileName.toUtf8().data(), pt);

    try
    {
      boost::property_tree::ptree legend = pt.get_child("Legend");

      std::string tranformation = legend.get<std::string>("Transformation");
      std::string type = legend.get<std::string>("Type");
      std::string slices = legend.get<std::string>("Slices");
      std::string precision = legend.get<std::string>("Precision");

      std::string catalog = legend.get<std::string>("Catalog");
      std::string group = legend.get<std::string>("Group");
      std::string schema = legend.get<std::string>("Schema");

      m_ui->m_transformComboBox->setCurrentIndex(m_ui->m_transformComboBox->findText(tranformation.c_str()));
      m_ui->m_typeComboBox->setCurrentIndex(m_ui->m_typeComboBox->findText(type.c_str()));
      m_ui->m_slicesSpinBox->setValue(boost::lexical_cast<double>(slices));
      m_ui->m_precisionSpinBox->setValue(boost::lexical_cast<double>(precision));

      m_colorBar->loadDefaultColorCatalog();

      m_colorBar->setCatalog(catalog);
      m_colorBar->setGroup(group);
      m_colorBar->setSchema(schema);

      std::vector<std::vector<std::string> > items;

      for(boost::property_tree::ptree::value_type &v: legend.get_child("Items"))
      {
        std::vector<std::string> item;
        item.push_back(v.second.get<std::string>("From"));
        item.push_back(v.second.get<std::string>("To"));
        item.push_back(v.second.get<std::string>("R"));
        item.push_back(v.second.get<std::string>("G"));
        item.push_back(v.second.get<std::string>("B"));

        items.push_back(item);
      }

      m_ui->m_tableWidget->setRowCount(items.size());

      for (std::size_t i = 0; i < items.size(); ++i)
      {
        m_ui->m_tableWidget->setItem(i, 1, new QTableWidgetItem(items[i][0].c_str()));
        m_ui->m_tableWidget->setItem(i, 2, new QTableWidgetItem(items[i][1].c_str()));

        QColor color;
        color.setRgb(boost::lexical_cast<int>(items[i][3]), boost::lexical_cast<int>(items[i][4]), boost::lexical_cast<int>(items[i][5]));

        QPixmap pix(24, 24);
        QPainter p(&pix);

        p.fillRect(0, 0, 12, 24, color);
        p.fillRect(12, 0, 12, 24, color);
        p.setBrush(Qt::transparent);
        p.setPen(Qt::black);
        p.drawRect(0, 0, 23, 23);

        QIcon icon(pix);

        QTableWidgetItem* item = new QTableWidgetItem(icon, "");
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_ui->m_tableWidget->setItem(i, 0, item);

      }
    }
    catch(boost::property_tree::ptree_error& e)
    {
      QMessageBox::warning(this, tr("Grouping"), tr("Attribute not found."));
      return;
    }
  }
  catch(boost::property_tree::json_parser::json_parser_error& e)
  {
    QMessageBox::warning(this, tr("Grouping"), tr("Invalid file"));
    return;
  }
}

void te::qt::widgets::ColorMapWidget::onSavePushButtonClicked()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save as..."),
    QString(), tr("LEG (*.leg *.LEG);;"), nullptr, QFileDialog::DontConfirmOverwrite);

  saveLegend(fileName.toUtf8().data());
}

void te::qt::widgets::ColorMapWidget::saveLegend(const std::string& path)
{
  int rowCount = m_ui->m_tableWidget->rowCount();

  if (rowCount < 1)
    return;

  if (path.empty())
    return;

  boost::property_tree::ptree pt;
  boost::property_tree::ptree legend;

  std::string tranformation = m_ui->m_transformComboBox->currentText().toUtf8().data();
  std::string type = m_ui->m_typeComboBox->currentText().toUtf8().data();
  std::string slices = m_ui->m_slicesSpinBox->text().toUtf8().data();
  std::string precision = m_ui->m_precisionSpinBox->text().toUtf8().data();

  std::string catalog = m_colorBar->getCatalog();
  std::string group = m_colorBar->getGroup();
  std::string schema = m_colorBar->getSchema();

  legend.add("Transformation", tranformation);
  legend.add("Type", type);
  legend.add("Slices", slices);
  legend.add("Precision", precision);
  legend.add("Catalog", catalog);
  legend.add("Group", group);
  legend.add("Schema", schema);

  boost::property_tree::ptree items;

  std::vector<te::color::RGBAColor> colorVec;

  int sliceValue = m_ui->m_slicesSpinBox->value();

  int index = m_ui->m_transformComboBox->currentIndex();

  int transType = m_ui->m_transformComboBox->itemData(index).toInt();

  if (transType == te::se::CATEGORIZE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(sliceValue);
  }
  else if (transType == te::se::INTERPOLATE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(sliceValue + 1);
  }
  else if (transType == te::se::RECODE_TRANSFORMATION)
  {
    colorVec = m_cb->getSlices(getRecodeValues().size());
  }

  for (std::size_t i = 0; i < m_ui->m_tableWidget->rowCount(); ++i)
  {
    std::string from = m_ui->m_tableWidget->item(i, 1)->text().toUtf8().data();
    std::string to = m_ui->m_tableWidget->item(i, 2)->text().toUtf8().data();

    boost::property_tree::ptree item;
    item.add("From", from);
    item.add("To", to);
    item.add("Red", colorVec.at(i).getRed());
    item.add("Green", colorVec.at(i).getGreen());
    item.add("Blue", colorVec.at(i).getBlue());

    items.add_child("Item", item);
  }

  legend.add_child("Items", items);

  pt.add_child("Legend", legend);

  boost::property_tree::write_json(path, pt);
}

std::vector<std::string> te::qt::widgets::ColorMapWidget::getValues()
{
  int band = m_ui->m_bandComboBox->currentText().toInt();

  unsigned int rows = m_raster->getNumberOfRows(),
      cols = m_raster->getNumberOfColumns();

  std::vector<std::string> values;

  for (unsigned int r = 0; r < rows; ++r)
  {
    for (unsigned int c = 0; c < cols; ++c)
    {
      double value;
      m_raster->getValue(c, r, value, static_cast<size_t>(band));
      std::string strValue = boost::lexical_cast<std::string>(value);
      
      if (std::find(values.begin(), values.end(), strValue) == values.end())
        values.push_back(strValue);
    }
  }

  return values;
}

std::vector<std::string> te::qt::widgets::ColorMapWidget::getRecodeValues()
{
  std::vector<std::string> values;
  
  int min = m_ui->m_minValueLineEdit->text().toInt();
  int max = m_ui->m_maxValueLineEdit->text().toInt();

  for (int i = min; i <= max; ++i)
  {
    values.push_back(boost::lexical_cast<std::string>(i));
  }

  return values;
}

void te::qt::widgets::ColorMapWidget::onTransformComboBoxCurrentIndexChanged(int index)
{
  te::se::ColorMapTransformationType t = (te::se::ColorMapTransformationType)m_ui->m_transformComboBox->itemData(index).toInt();

  if (t == te::se::RECODE_TRANSFORMATION)
  {
    m_ui->m_minValueLineEdit->setEnabled(true);
    m_ui->m_maxValueLineEdit->setEnabled(true);
    m_ui->m_slicesSpinBox->setEnabled(false);
    m_ui->m_precisionSpinBox->setEnabled(false);

    m_ui->m_typeComboBox->setCurrentIndex(1); // Unique Value
    m_ui->m_typeComboBox->setEnabled(false);

  }
  else
  {
    m_ui->m_minValueLineEdit->setEnabled(true);
    m_ui->m_maxValueLineEdit->setEnabled(true);
    m_ui->m_slicesSpinBox->setEnabled(true);
    m_ui->m_precisionSpinBox->setEnabled(true);

    m_ui->m_typeComboBox->setCurrentIndex(0); // Equal Steps
    m_ui->m_typeComboBox->setEnabled(false);
  }
}

void te::qt::widgets::ColorMapWidget::updateTableHeader(te::se::ColorMapTransformationType type)
{
  if (type == te::se::RECODE_TRANSFORMATION)
  {
    m_ui->m_tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem(tr("Color")));
    m_ui->m_tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem(tr("Value")));
    m_ui->m_tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem(tr("Title")));
  }
  else
  {
    m_ui->m_tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem(tr("Color")));
    m_ui->m_tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem(tr("From")));
    m_ui->m_tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem(tr("To")));
  }
}
