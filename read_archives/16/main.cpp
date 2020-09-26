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
  \file terralib/qt/widgets/se/GroupingWidget.cpp

  \brief A widget used to build a grouping.
*/

// TerraLib
#include "../../../color/ColorBar.h"
#include "../../../common/Globals.h"
#include "../../../common/STLUtils.h"
#include "../../../dataaccess/datasource/DataSource.h"
#include "../../../dataaccess/query/SQLVisitor.h"
#include "../../../dataaccess/dataset/DataSet.h"
#include "../../../dataaccess/dataset/DataSetType.h"
#include "../../../dataaccess/query/OrderByItem.h"
#include "../../../dataaccess/utils/Utils.h"
#include "../../../fe/Filter.h"
#include "../../../fe/Utils.h"
#include "../../../geometry/GeometryProperty.h"
#include "../../../maptools/Enums.h"
#include "../../../maptools/GroupingAlgorithms.h"
#include "../../../maptools/Grouping.h"
#include "../../../maptools/Utils.h"
#include "../../../maptools/QueryEncoder.h"
#include "../../../maptools/QueryLayer.h"
#include "../../../se.h"
#include "../../../se/Symbolizer.h"
#include "../../../se/SymbolizerColorFinder.h"
#include "../../../se/Style.h"
#include "../../../se/Utils.h"
#include "../colorbar/ColorBar.h"
#include "../colorbar/ColorCatalogWidget.h"
#include "../se/LineSymbolizerWidget.h"
#include "../se/PointSymbolizerWidget.h"
#include "../se/PolygonSymbolizerWidget.h"
#include "../se/SymbologyPreview.h"
#include "GroupingWidget.h"
#include "ui_GroupingWidgetForm.h"

// STL
#include <cassert>

// QT
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>

#define MAX_SLICES 200
#define PRECISION 15
#define NO_TITLE "No Value"

Q_DECLARE_METATYPE(te::map::AbstractLayerPtr)

std::string GetRuleDescription(te::se::Rule* rule, te::da::DataSourcePtr ds)
{
  std::string sql = "";

  te::map::QueryEncoder filter2Query;

  te::da::Expression* exp = filter2Query.getExpression(rule->getFilter());

  te::da::SQLVisitor visitor(*ds->getDialect(), sql);

  try
  {
    exp->accept(visitor);
  }
  catch (...)
  {
    return "";
  }

  return sql;
}

te::qt::widgets::GroupingWidget::GroupingWidget(QWidget* parent, Qt::WindowFlags f)
  : QWidget(parent, f),
    m_ui(new Ui::GroupingWidgetForm),
    m_cb(nullptr)
{
  m_ui->setupUi(this);

  QGridLayout* l = new QGridLayout(m_ui->m_colorBarWidget);
  l->setContentsMargins(0,0,0,0);
  m_colorBar = new  te::qt::widgets::ColorCatalogWidget(m_ui->m_colorBarWidget);
  l->addWidget(m_colorBar);

//connects
  connect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onColorBarChanged()));
  connect(m_ui->m_typeComboBox, SIGNAL(activated(int)), this, SLOT(onTypeComboBoxActivated(int)));
  connect(m_ui->m_attrComboBox, SIGNAL(activated(int)), this, SLOT(onAttrComboBoxActivated(int)));
  connect(m_ui->m_applyPushButton, SIGNAL(clicked()), this, SLOT(onApplyPushButtonClicked()));
  connect(m_ui->m_tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onTableWidgetItemDoubleClicked(QTableWidgetItem*)));
  connect(m_ui->m_importPushButton, SIGNAL(clicked()), this, SLOT(onImportPushButtonClicked()));
  connect(m_ui->m_loadLegendPushButton, SIGNAL(clicked()), this, SLOT(onLoadPushButtonClicked()));
  connect(m_ui->m_saveLegendPushButton, SIGNAL(clicked()), this, SLOT(onSavePushButtonClicked()));

  //m_importGroupingGroupBox
  initialize();
}

te::qt::widgets::GroupingWidget::~GroupingWidget()
{
  delete m_cb;

  te::common::FreeContents(m_rules);
  m_rules.clear();
}

std::unique_ptr<te::map::Grouping> te::qt::widgets::GroupingWidget::getGrouping()
{
  if (m_ui->m_importGroupBox->isChecked())
  {
    QVariant varLayer = m_ui->m_layersComboBox->itemData(m_ui->m_layersComboBox->currentIndex(), Qt::UserRole);
    te::map::AbstractLayerPtr layer = varLayer.value<te::map::AbstractLayerPtr>();

    te::map::Grouping* ref = layer->getGrouping();

    std::unique_ptr<te::map::Grouping> group(new te::map::Grouping(*ref));

    return group;
  }

  std::string attr = m_ui->m_attrComboBox->currentText().toUtf8().data();
  int attrIdx = m_ui->m_attrComboBox->currentIndex();
  int attrType = m_ui->m_attrComboBox->itemData(attrIdx).toInt();

  int index = m_ui->m_typeComboBox->currentIndex();
  int type = m_ui->m_typeComboBox->itemData(index).toInt();

  std::unique_ptr<te::map::Grouping> group(new te::map::Grouping(attr, (te::map::GroupingType)type));

  group->setPropertyType(attrType);

  group->setNumSlices(m_ui->m_slicesSpinBox->value());

  group->setPrecision(m_ui->m_precSpinBox->value());

  group->setStdDeviation(m_ui->m_stdDevDoubleSpinBox->value());

  group->setSummary(m_ui->m_summaryComboBox->currentText().toUtf8().data());

  return group;
}

void te::qt::widgets::GroupingWidget::updateStyle()
{
  if (m_layer)
  {
    te::da::DataSourcePtr ds = te::da::GetDataSource(m_layer->getDataSourceId());

    te::se::Style* style = m_layer->getStyle();

    //set style name
    std::string prefixName = tr("Grouping Style: ").toUtf8().data();
    std::string* styleName = new std::string(prefixName + m_ui->m_typeComboBox->currentText().toUtf8().data());
    style->setName(styleName);

    //set style description
    QString descInfo = QObject::tr("Grouping Information") + ":\n";

    descInfo += QObject::tr("\tProperty: ") + m_ui->m_attrComboBox->currentText() + "\n";
    descInfo += QObject::tr("\tNum Slices: ") + QString::number(m_ui->m_slicesSpinBox->value()) + "\n";
    descInfo += QObject::tr("\tPrecision: ") + QString::number(m_ui->m_precSpinBox->value()) + "\n";
    descInfo += QObject::tr("\tStd Deviation: ") + QString::number(m_ui->m_stdDevDoubleSpinBox->value()) + "\n";
    descInfo += QObject::tr("\tSumary: ") + m_ui->m_summaryComboBox->currentText();

    te::se::Description* descStyle = new te::se::Description;
    descStyle->setTitle(descInfo.toUtf8().data());
    style->setDescription(descStyle);

    style->removeRules();

    for (std::size_t t = 0; t < m_rules.size(); ++t)
    {
      te::se::Rule* rule = m_rules[t];

      //set rule description
      std::string descStr = GetRuleDescription(rule, ds);
      te::se::Description* desc = new te::se::Description;
      desc->setTitle(descStr);
      rule->setDescription(desc);

      style->push_back(rule);
    }

    m_rules.clear();
  }
}

void te::qt::widgets::GroupingWidget::initialize()
{
  // create color bar
  m_colorBar->getColorBar()->setHeight(20);
  m_colorBar->getColorBar()->setScaleVisible(false);

  // fill grouping type combo box
  m_ui->m_typeComboBox->addItem(tr("Equal Steps"), te::map::EQUAL_STEPS);
  m_ui->m_typeComboBox->addItem(tr("Quantil"), te::map::QUANTIL);
  m_ui->m_typeComboBox->addItem(tr("Standard Deviation"), te::map::STD_DEVIATION);
  m_ui->m_typeComboBox->addItem(tr("Unique Value"), te::map::UNIQUE_VALUE);

  onTypeComboBoxActivated(0);

  //set number of slices
  m_ui->m_slicesSpinBox->setMinimum(1);
  m_ui->m_slicesSpinBox->setMaximum(MAX_SLICES);
  m_ui->m_slicesSpinBox->setValue(5);
  m_ui->m_slicesSpinBox->setSingleStep(1);

  //set standard deviation values
  m_ui->m_stdDevDoubleSpinBox->setMinimum(0.25);
  m_ui->m_stdDevDoubleSpinBox->setMaximum(1.0);
  m_ui->m_stdDevDoubleSpinBox->setValue(0.5);
  m_ui->m_stdDevDoubleSpinBox->setSingleStep(0.25);

  //set number of precision
  m_ui->m_precSpinBox->setMinimum(1);
  m_ui->m_precSpinBox->setMaximum(PRECISION);
  m_ui->m_precSpinBox->setValue(6);
  m_ui->m_precSpinBox->setSingleStep(1);

  //adjust table
#if (QT_VERSION >= 0x050000)
  m_ui->m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
  m_ui->m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

  m_manual = false;
}

void te::qt::widgets::GroupingWidget::updateUi(bool loadColorBar)
{
  disconnect(m_ui->m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onTableWidgetItemChanged(QTableWidgetItem*)));

  m_ui->m_tableWidget->setRowCount(0);

  int index = m_ui->m_typeComboBox->currentIndex();
  int type = m_ui->m_typeComboBox->itemData(index).toInt();

  if(type == te::map::EQUAL_STEPS || type == te::map::QUANTIL || type == te::map::STD_DEVIATION)
  {
    QStringList list;
    list.append(tr("Symbol"));
    list.append(tr("Title"));
    list.append(tr("Min"));
    list.append(tr("Max"));

    m_ui->m_tableWidget->setColumnCount(4);
    m_ui->m_tableWidget->setHorizontalHeaderLabels(list);
  }
  else if(type == te::map::UNIQUE_VALUE)
  {
    QStringList list;
    list.append(tr("Symbol"));
    list.append(tr("Title"));
    list.append(tr("Value"));

    m_ui->m_tableWidget->setColumnCount(3);
    m_ui->m_tableWidget->setHorizontalHeaderLabels(list);
  }

  te::color::ColorBar* cb = nullptr;

  if(loadColorBar)
  {
    if (!m_rules.empty() && !m_rules[0]->getSymbolizers().empty() && !m_rules[m_rules.size() - 1]->getSymbolizers().empty())
    {
      te::se::SymbolizerColorFinder scf;

      scf.find(m_rules[0]->getSymbolizers()[0]);
      te::color::RGBAColor initColor = scf.getColor();

      scf.find(m_rules[m_rules.size() - 1]->getSymbolizers()[0]);
      te::color::RGBAColor endColor = scf.getColor();

      cb = new te::color::ColorBar(initColor, endColor, 256);
    }
  }

  int count = 0;

  for(std::size_t t = 0; t < m_rules.size(); ++t)
  {
    te::se::Rule* ruleItem = m_rules[t];
    const te::fe::Filter* ruleFilter = ruleItem->getFilter();

    if (!ruleFilter)
      continue;

    int newrow = m_ui->m_tableWidget->rowCount();
    m_ui->m_tableWidget->insertRow(newrow);

    //symbol
    {
      const std::vector<te::se::Symbolizer*>& ss = ruleItem->getSymbolizers();
      QPixmap pix = te::qt::widgets::SymbologyPreview::build(ss, QSize(24, 24));
      QIcon icon(pix);
      QTableWidgetItem* item = new QTableWidgetItem(icon, "");
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      m_ui->m_tableWidget->setItem(newrow, 0, item);
    }

    //title
    {
      QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8((*ruleItem->getName()).c_str()));
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
      m_ui->m_tableWidget->setItem(newrow, 1, item);
    }

    if(loadColorBar)
    {
      if (count != 0 && count != static_cast<int>(m_rules.size()) - 1)
      {
        double pos = (1. / (m_rules.size() - 1)) * count;

        if (!ruleItem->getSymbolizers().empty())
        {
          te::se::SymbolizerColorFinder scf;

          scf.find(ruleItem->getSymbolizers()[0]);

          te::color::RGBAColor color = scf.getColor();

          if(cb)
            cb->addColor(color, pos);
        }
      }
    }

    ++count;

    std::string ruleName = *ruleItem->getName();

    if(type == te::map::EQUAL_STEPS || type == te::map::QUANTIL || type == te::map::STD_DEVIATION)
    {  
      if (ruleName != NO_TITLE)
      {
        //Values
        std::string valueMin = "";
        std::string valueMax = "";
        te::fe::GetFilterStepValues(ruleFilter, valueMin, valueMax);

        //Min
        {
          QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8(valueMin.c_str()));
          item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
          m_ui->m_tableWidget->setItem(newrow, 2, item);
        }

        //Max
        {
          QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8(valueMax.c_str()));
          item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
          m_ui->m_tableWidget->setItem(newrow, 3, item);
        }
      }
    }
    else if(type == te::map::UNIQUE_VALUE)
    {
      if (ruleName != NO_TITLE)
      {
        //Value
        std::string value = "";
        te::fe::GetFilterUniqueValue(ruleFilter, value);

        QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8(value.c_str()));
        item->setFlags(Qt::ItemIsEnabled);
        m_ui->m_tableWidget->setItem(newrow, 2, item);
      }
    }
  }

  if(cb)
  {
    disconnect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onColorBarChanged()));

    te::qt::widgets::colorbar::ColorBar* cbW = m_colorBar->getColorBar();
    cbW->setColorBar(cb);

    connect(m_colorBar, SIGNAL(colorBarChanged()), this, SLOT(onColorBarChanged()));
  }

#if (QT_VERSION >= 0x050000)
  m_ui->m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#else
  m_ui->m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#endif

  connect(m_ui->m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onTableWidgetItemChanged(QTableWidgetItem*)));
}

void te::qt::widgets::GroupingWidget::setDataSetType()
{
  listAttributes();
}

void te::qt::widgets::GroupingWidget::setGrouping()
{
  if (m_layer)
  {
    te::map::Grouping* grouping = m_layer->getGrouping();

    if (grouping)
    {
      setGrouping(grouping, m_layer->getStyle());

      emit applyPushButtonClicked();
    }
  }
}

void te::qt::widgets::GroupingWidget::setGrouping(te::map::Grouping* grouping, te::se::Style* style)
{
  if (!grouping)
    return;

  //type
  te::map::GroupingType type = grouping->getType();

  for (int i = 0; i < m_ui->m_typeComboBox->count(); ++i)
  {
    if (type == m_ui->m_typeComboBox->itemData(i).toInt())
    {
      m_ui->m_typeComboBox->setCurrentIndex(i);

      onTypeComboBoxActivated(i);

      break;
    }
  }

  //attr name
  std::string attrName = grouping->getPropertyName();

  for (int i = 0; i < m_ui->m_attrComboBox->count(); ++i)
  {
    if (attrName == m_ui->m_attrComboBox->itemText(i).toUtf8().data())
    {
      m_ui->m_attrComboBox->setCurrentIndex(i);
      break;
    }
  }

  //precision
  size_t prec = grouping->getPrecision();

  m_ui->m_precSpinBox->setValue((int)prec);

  //slices
  size_t slices = grouping->getNumSlices();

  m_ui->m_slicesSpinBox->setValue((int)slices);

  //std dev
  float stdDev = grouping->getStdDeviation();

  m_ui->m_stdDevDoubleSpinBox->setValue((double)stdDev);

  //grouping items
  te::common::FreeContents(m_rules);
  m_rules.clear();

  for (size_t t = 0; t < style->getRules().size(); ++t)
  {
    te::se::Rule* rule = style->getRule(t)->clone();

    m_rules.push_back(rule);
  }

  updateUi(true);
}

void te::qt::widgets::GroupingWidget::onApplyPushButtonClicked()
{
  if(m_manual)
  {
      int reply = QMessageBox::question(this, tr("Edit Legend"), tr("Manual changes will be lost. Continue?"), QMessageBox::Yes | QMessageBox::Cancel);

      if(reply != QMessageBox::Yes)
        return;
  }

  int index = m_ui->m_typeComboBox->currentIndex();

  int type = m_ui->m_typeComboBox->itemData(index).toInt();
  int slices = m_ui->m_slicesSpinBox->value();
  int prec = m_ui->m_precSpinBox->value();
  double stdDev = m_ui->m_stdDevDoubleSpinBox->value();

  std::string attr = m_ui->m_attrComboBox->currentText().toUtf8().data();
  int attrIdx =  m_ui->m_attrComboBox->currentIndex();
  int attrType = m_ui->m_attrComboBox->itemData(attrIdx).toInt();

  te::common::FreeContents(m_rules);
  m_rules.clear();

  std::string mean = "";

  int nullValues = 0;

  bool update = false;

  if(type == te::map::EQUAL_STEPS)
  {
    std::vector<double> vec;

    getDataAsDouble(vec, attr, attrType, nullValues);

    if (!vec.empty())
    {
      te::map::GroupingByEqualSteps(attr, vec.begin(), vec.end(), slices, m_rules, prec);

      buildSymbolizer(mean);

      createDoubleNullGroupingItem(nullValues);
    }
  }
  else if(type == te::map::QUANTIL) 
  {
    std::vector<double> vec;

    getDataAsDouble(vec, attr, attrType, nullValues);

    if (!vec.empty())
    {
      te::map::GroupingByQuantil(attr, vec.begin(), vec.end(), slices, m_rules, prec);

      buildSymbolizer(mean);

      createDoubleNullGroupingItem(nullValues);
    }
  }
  else if(type == te::map::STD_DEVIATION) 
  {
    std::vector<double> vec;

    getDataAsDouble(vec, attr, attrType, nullValues);

    if (!vec.empty())
    {
      te::map::GroupingByStdDeviation(attr, vec.begin(), vec.end(), stdDev, m_rules, mean, prec);

      buildSymbolizer(mean);

      createDoubleNullGroupingItem(nullValues);
    }

    update = false;
  }
  else if(type == te::map::UNIQUE_VALUE) 
  {
    std::vector<std::string> vec;

    getDataAsString(vec, attr, nullValues);

    if (!vec.empty())
    {
      te::map::GroupingByUniqueValues(attr, vec, attrType, m_rules, prec);

      buildSymbolizer(mean);

      createStringNullGroupingItem(nullValues);
    }
  }

  updateUi(update);

  m_manual = false;

  emit applyPushButtonClicked();
}

void te::qt::widgets::GroupingWidget::onTypeComboBoxActivated(int idx)
{
  int type = m_ui->m_typeComboBox->itemData(idx).toInt();

  if(type == te::map::EQUAL_STEPS)
  {
    m_ui->m_slicesSpinBox->setEnabled(true);
    m_ui->m_precSpinBox->setEnabled(true);
    m_ui->m_stdDevDoubleSpinBox->setEnabled(false);
  }
  else if(type == te::map::QUANTIL) 
  {
    m_ui->m_slicesSpinBox->setEnabled(true);
    m_ui->m_precSpinBox->setEnabled(true);
    m_ui->m_stdDevDoubleSpinBox->setEnabled(false);
  }
  else if(type == te::map::STD_DEVIATION) 
  {
    m_ui->m_slicesSpinBox->setEnabled(false);
    m_ui->m_precSpinBox->setEnabled(true);
    m_ui->m_stdDevDoubleSpinBox->setEnabled(true);
  }
  else if(type == te::map::UNIQUE_VALUE) 
  {
    m_ui->m_slicesSpinBox->setEnabled(false);
    m_ui->m_precSpinBox->setEnabled(true);
    m_ui->m_stdDevDoubleSpinBox->setEnabled(false);
  }

  if(m_layer.get())
    listAttributes();
}

void te::qt::widgets::GroupingWidget::onAttrComboBoxActivated(int idx)
{

}

void te::qt::widgets::GroupingWidget::onColorBarChanged()
{
  if(m_layer.get())
  {
    buildSymbolizer();

    updateUi();
  }
}

void  te::qt::widgets::GroupingWidget::onTableWidgetItemChanged(QTableWidgetItem* item)
{
  int index = m_ui->m_typeComboBox->currentIndex();
  int type = m_ui->m_typeComboBox->itemData(index).toInt();

  int curRow = m_ui->m_tableWidget->currentRow();
  int curCol = m_ui->m_tableWidget->currentColumn();

  std::string attrName = m_ui->m_attrComboBox->currentText().toUtf8().data();

  QString str = item->text();

  if(curCol == 1) // title
  {
    std::string ruleName = str.toUtf8().data();
    m_rules[curRow]->setName(&ruleName);

    m_manual = true;
  }
  else if(curCol == 2 || curCol == 3) // min and max
  {
    if(type == te::map::EQUAL_STEPS || type == te::map::QUANTIL || type == te::map::STD_DEVIATION)
    {
      bool ok = false;

      str.toDouble(&ok);

      if(!ok)
      {
        std::string valueMin = "";
        std::string valueMax = "";
        const te::fe::Filter* ruleFilter = m_rules[curRow]->getFilter();
        te::fe::GetFilterStepValues(ruleFilter, valueMin, valueMax);

        if(curCol == 2)
          item->setText(valueMin.c_str());
        else if(curCol ==3)
          item->setText(valueMax.c_str());
      }
      else
      {
        std::string valueMin = "";
        std::string valueMax = "";
        const te::fe::Filter* ruleFilter = m_rules[curRow]->getFilter();
        te::fe::GetFilterStepValues(ruleFilter, valueMin, valueMax);

        if(curCol == 2)
          valueMin = item->text().toUtf8().data();
        else if(curCol ==3)
          valueMax = item->text().toUtf8().data();

        m_rules[curRow]->setFilter(te::fe::CreateFilterByStep(attrName, valueMin, valueMax));

        m_manual = true;
      }
    }
  }
}

void te::qt::widgets::GroupingWidget::onTableWidgetItemDoubleClicked(QTableWidgetItem* item)
{
  int curRow = m_ui->m_tableWidget->currentRow();
  int curCol = m_ui->m_tableWidget->currentColumn();

  if(curCol == 0)
  {
    te::se::Rule* ruleItem = m_rules[curRow];

    std::vector<te::se::Symbolizer*> symbVec = ruleItem->getSymbolizers();

    QDialog* dialog = new QDialog(this);
    QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, dialog);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, dialog);

    QWidget* symbWidget = nullptr;

    if(symbVec[0]->getType() == "PolygonSymbolizer")
    {
      symbWidget = new te::qt::widgets::PolygonSymbolizerWidget(dialog);
      te::qt::widgets::PolygonSymbolizerWidget* polygonSymbolizerWidget = (te::qt::widgets::PolygonSymbolizerWidget*)symbWidget;
      polygonSymbolizerWidget->setSymbolizer((te::se::PolygonSymbolizer*)symbVec[0]);
    }
    else if(symbVec[0]->getType() == "LineSymbolizer")
    {
      symbWidget = new te::qt::widgets::LineSymbolizerWidget(dialog);
      te::qt::widgets::LineSymbolizerWidget* lineSymbolizerWidget = (te::qt::widgets::LineSymbolizerWidget*)symbWidget;
      lineSymbolizerWidget->setSymbolizer((te::se::LineSymbolizer*)symbVec[0]);
    }
    else if(symbVec[0]->getType() == "PointSymbolizer")
    {
      symbWidget = new te::qt::widgets::PointSymbolizerWidget(dialog);
      te::qt::widgets::PointSymbolizerWidget* pointSymbolizerWidget = (te::qt::widgets::PointSymbolizerWidget*)symbWidget;
      pointSymbolizerWidget->setSymbolizer((te::se::PointSymbolizer*)symbVec[0]);
    }

    layout->addWidget(symbWidget);
    layout->addWidget(bbox);

    connect(bbox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(bbox, SIGNAL(rejected()), dialog, SLOT(reject()));

    if(dialog->exec() == QDialog::Rejected)
    {
      delete dialog;
      return;
    }

    if(symbVec[0]->getType() == "PolygonSymbolizer")
    {
      symbVec.clear();
      te::qt::widgets::PolygonSymbolizerWidget* polygonSymbolizerWidget = (te::qt::widgets::PolygonSymbolizerWidget*)symbWidget;
      symbVec.push_back(polygonSymbolizerWidget->getSymbolizer());
    }
    else if(symbVec[0]->getType() == "LineSymbolizer")
    {
      symbVec.clear();
      te::qt::widgets::LineSymbolizerWidget* lineSymbolizerWidget = (te::qt::widgets::LineSymbolizerWidget*)symbWidget;
      symbVec.push_back(lineSymbolizerWidget->getSymbolizer());
    }
    else if(symbVec[0]->getType() == "PointSymbolizer")
    {
      symbVec.clear();
      te::qt::widgets::PointSymbolizerWidget* pointSymbolizerWidget = (te::qt::widgets::PointSymbolizerWidget*)symbWidget;
      symbVec.push_back(pointSymbolizerWidget->getSymbolizer());
    }

    ruleItem->setSymbolizers(symbVec);

    QPixmap pix = te::qt::widgets::SymbologyPreview::build(symbVec, QSize(24, 24));
    QIcon icon(pix);

    QTableWidgetItem* newItem = new QTableWidgetItem(icon, "");
    newItem->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);

    m_ui->m_tableWidget->setItem(curRow, 0, newItem);

    delete dialog;

    updateUi(true);

    emit applyPushButtonClicked();
  }
}


void te::qt::widgets::GroupingWidget::getDataAsDouble(std::vector<double>& vec, const std::string& attrName, const int& dataType, int& nullValues)
{
  assert(m_layer.get());

  if (te::da::HasLinkedTable(m_layer->getSchema().get()) && (m_ui->m_summaryComboBox->currentText().toStdString() != "NONE"))
  {
    getLinkedDataAsDouble(vec, attrName, dataType, nullValues);
    return;
  }

  std::unique_ptr<te::map::LayerSchema> dsType(m_layer->getSchema());

  std::size_t idx=std::string::npos;

  for(std::size_t t = 0; t < dsType->getProperties().size(); ++t)
  {
    if(dsType->getProperty(t)->getName() == attrName)
    {
      idx = t;
      break;
    }
  }

  std::unique_ptr<te::da::DataSet> ds(m_layer->getData());

  ds->moveBeforeFirst();

  while(ds->moveNext())
  {
    if(ds->isNull(idx))
    {
      ++nullValues;
      continue;
    }

    if(dataType == te::dt::INT16_TYPE)
      vec.push_back((double)ds->getInt16(idx));
    else if(dataType == te::dt::INT32_TYPE)
      vec.push_back((double)ds->getInt32(idx));
    else if(dataType == te::dt::INT64_TYPE)
      vec.push_back((double)ds->getInt64(idx));
    else if(dataType == te::dt::FLOAT_TYPE)
      vec.push_back((double)ds->getFloat(idx));
    else if(dataType == te::dt::DOUBLE_TYPE)
      vec.push_back(ds->getDouble(idx));
    else if(dataType == te::dt::NUMERIC_TYPE)
    {
      QString strNum = ds->getNumeric(idx).c_str();

      bool ok = false;

      double value = strNum.toDouble(&ok);

      if(ok)
        vec.push_back(value);
    }
  }
}

void te::qt::widgets::GroupingWidget::getLinkedDataAsDouble(std::vector<double>& vec, const std::string& attrName, const int& dataType, int& nullValues)
{
  assert(m_layer.get());

  std::unique_ptr<te::map::LayerSchema> dsType(m_layer->getSchema());

  std::string function = m_ui->m_summaryComboBox->currentText().toUtf8().data();
  std::vector<std::string> poid;
  size_t pksize = 0;
  te::map::QueryLayer* qlayer = nullptr;
  te::da::Select* select = nullptr;

  // make sorting by object id
  qlayer = dynamic_cast<te::map::QueryLayer*>(m_layer.get());
  select = dynamic_cast<te::da::Select*>(qlayer->getQuery()->clone());
  te::da::Select* selectaux = dynamic_cast<te::da::Select*>(select->clone());
  te::da::OrderBy* orderBy = new te::da::OrderBy;

  std::vector<te::dt::Property*> props = dsType->getPrimaryKey()->getProperties();
  while(++pksize < props.size())
  {
    poid.push_back(props[pksize-1]->getName());
    if(props[pksize-1]->getDatasetName() != props[pksize]->getDatasetName())
      break;
  }

  for(size_t i = 0; i < pksize; ++i)
    orderBy->push_back(new te::da::OrderByItem(poid[i]));

  selectaux->setOrderBy(orderBy);
  qlayer->setQuery(selectaux);

  std::size_t idx=std::string::npos;

  for(std::size_t t = 0; t < dsType->getProperties().size(); ++t)
  {
    if(dsType->getProperty(t)->getName() == attrName)
    {
      idx = t;
      break;
    }
  }

  std::vector<std::string> pkdata(pksize), pkdataaux(pksize);  
  std::unique_ptr<te::da::DataSet> ds(m_layer->getData());
  qlayer->setQuery(select);

  bool nullValue = false;
  std::vector<double> values;
  bool isBegin = true;
  ds->moveBeforeFirst();

  while(ds->moveNext())
  {
    if(pksize)
    {
      // it is linked. Remove redundancies.
      size_t i;
      for(i = 0; i < pksize; ++i)
      {
        pkdata[i] = ds->getAsString(poid[i]);
        if(isBegin)
        {
          isBegin = false;
          pkdataaux[i] = ds->getAsString(poid[i]);
        }
      }

      for(i = 0; i < pksize; ++i)
      {
        if(pkdata[i] != pkdataaux[i])
        {
          pkdataaux = pkdata;
          break;
        }
      }
      if(i == pksize) // it is the same object
      {
        if(nullValue == false)
        {
          if(ds->isNull(idx))
            nullValue = true;
          else
          {
            if(dataType == te::dt::INT16_TYPE)
              values.push_back((double)ds->getInt16(idx));
            else if(dataType == te::dt::INT32_TYPE)
              values.push_back((double)ds->getInt32(idx));
            else if(dataType == te::dt::INT64_TYPE)
              values.push_back((double)ds->getInt64(idx));
            else if(dataType == te::dt::FLOAT_TYPE)
              values.push_back((double)ds->getFloat(idx));
            else if(dataType == te::dt::DOUBLE_TYPE)
              values.push_back(ds->getDouble(idx));
            else if(dataType == te::dt::NUMERIC_TYPE)
            {
              QString strNum = ds->getNumeric(idx).c_str();

              bool ok = false;

              double value = strNum.toDouble(&ok);

              if(ok)
                values.push_back(value);
            }
          }
        }
        continue;
        // read other values
      }
      else // it is other object
      {
        // sumarize value according to the required summarization 
        if(nullValue)
           ++nullValues;
        else
         vec.push_back(te::da::GetSummarizedValue(values, function));

        nullValue = false;
        values.clear();

        // get new value
        if(ds->isNull(idx))
          nullValue = true;
        else
        {
          if(dataType == te::dt::INT16_TYPE)
            values.push_back((double)ds->getInt16(idx));
          else if(dataType == te::dt::INT32_TYPE)
            values.push_back((double)ds->getInt32(idx));
          else if(dataType == te::dt::INT64_TYPE)
            values.push_back((double)ds->getInt64(idx));
          else if(dataType == te::dt::FLOAT_TYPE)
            values.push_back((double)ds->getFloat(idx));
          else if(dataType == te::dt::DOUBLE_TYPE)
            values.push_back(ds->getDouble(idx));
          else if(dataType == te::dt::NUMERIC_TYPE)
          {
            QString strNum = ds->getNumeric(idx).c_str();

            bool ok = false;

            double value = strNum.toDouble(&ok);

            if(ok)
              values.push_back(value);
          }
        }
      }
    }
  }
  // sumarize value according to the required summarization 
  if(nullValue)
    ++nullValues;
  else
    vec.push_back(te::da::GetSummarizedValue(values, function));
  values.clear();
}

void te::qt::widgets::GroupingWidget::getDataAsString(std::vector<std::string>& vec, const std::string& attrName, int& nullValues)
{
  assert(m_layer.get());

  if (te::da::HasLinkedTable(m_layer->getSchema().get()) && (m_ui->m_summaryComboBox->currentText().toStdString() != "NONE"))
  {
    getLinkedDataAsString(vec, attrName,  nullValues);
    return;
  }

  std::unique_ptr<te::map::LayerSchema> dsType(m_layer->getSchema());

  std::size_t idx=std::string::npos;

  for(std::size_t t = 0; t < dsType->getProperties().size(); ++t)
  {
    if(dsType->getProperty(t)->getName() == attrName)
    {
      idx = t;
      break;
    }
  }

  std::unique_ptr<te::da::DataSet> ds(m_layer->getData());

  ds->moveBeforeFirst();

  while(ds->moveNext())
  {
    if(!ds->isNull(idx))
      vec.push_back(ds->getAsString(idx));
    else
      ++nullValues;
  }
}

void te::qt::widgets::GroupingWidget::getLinkedDataAsString(std::vector<std::string>& vec, const std::string& attrName, int& nullValues)
{
  assert(m_layer.get());

  std::unique_ptr<te::map::LayerSchema> dsType(m_layer->getSchema());

  std::string function = m_ui->m_summaryComboBox->currentText().toUtf8().data();
  std::vector<std::string> poid;
  size_t pksize = 0;
  te::map::QueryLayer* qlayer = nullptr;
  te::da::Select* select = nullptr;

  // make sorting by object id
  qlayer = dynamic_cast<te::map::QueryLayer*>(m_layer.get());
  select = dynamic_cast<te::da::Select*>(qlayer->getQuery()->clone());
  te::da::Select* selectaux = dynamic_cast<te::da::Select*>(select->clone());
  te::da::OrderBy* orderBy = new te::da::OrderBy;

  std::vector<te::dt::Property*> props = dsType->getPrimaryKey()->getProperties();
  while(++pksize < props.size())
  {
    poid.push_back(props[pksize-1]->getName());
    if(props[pksize-1]->getDatasetName() != props[pksize]->getDatasetName())
      break;
  }

  for(size_t i = 0; i < pksize; ++i)
    orderBy->push_back(new te::da::OrderByItem(poid[i]));

  selectaux->setOrderBy(orderBy);
  qlayer->setQuery(selectaux);

  std::size_t idx=std::string::npos;

  for(std::size_t t = 0; t < dsType->getProperties().size(); ++t)
  {
    if(dsType->getProperty(t)->getName() == attrName)
    {
      idx = t;
      break;
    }
  }

  std::vector<std::string> pkdata(pksize), pkdataaux(pksize);  
  std::unique_ptr<te::da::DataSet> ds(m_layer->getData());
  qlayer->setQuery(select);

  bool nullValue = false;
  std::vector<std::string> values;
  bool isBegin = true;
  ds->moveBeforeFirst();

  while(ds->moveNext())
  {
    if(pksize)
    {
      // it is linked. Remove redundancies.
      size_t i;
      for(i = 0; i < pksize; ++i)
      {
        pkdata[i] = ds->getAsString(poid[i]);
        if(isBegin)
        {
          isBegin = false;
          pkdataaux[i] = ds->getAsString(poid[i]);
        }
      }

      for(i = 0; i < pksize; ++i)
      {
        if(pkdata[i] != pkdataaux[i])
        {
          pkdataaux = pkdata;
          break;
        }
      }
      if(i == pksize) // it is the same object
      {
        if(nullValue == false)
        {
          if(ds->isNull(idx))
            nullValue = true;
          else
            values.push_back(ds->getAsString(idx));
        }
        continue;
        // read other values
      }
      else // it is other object
      {
        // sumarize value according to the required summarization 
        if(nullValue)
          ++nullValues;
        else
          vec.push_back(te::da::GetSummarizedValue(values, function));

        nullValue = false;
        values.clear();

        // get new value
        if(ds->isNull(idx))
          nullValue = true;
        else
          values.push_back(ds->getAsString(idx));
      }
    }
  }
  // sumarize value according to the required summarization 
  if(nullValue)
    ++nullValues;
  else
    vec.push_back(te::da::GetSummarizedValue(values, function));
  values.clear();
}

void te::qt::widgets::GroupingWidget::createDoubleNullGroupingItem(int count)
{
  if(count == 0)
    return;

  std::string attrName = m_ui->m_attrComboBox->currentText().toUtf8().data();

  std::string* ruleName = new std::string(NO_TITLE);
  
  te::se::Rule* rule = new te::se::Rule;
  rule->setName(ruleName);
  rule->setFilter(te::fe::CreateFilterByStep(attrName, te::common::Globals::sm_nanStr, te::common::Globals::sm_nanStr));

  int geomType = getGeometryType();
  std::vector<te::se::Symbolizer*> symbVec;
  te::se::Symbolizer* s = te::se::CreateSymbolizer((te::gm::GeomType)geomType, "#dddddd");
  symbVec.push_back(s);
  rule->setSymbolizers(symbVec);

  m_rules.push_back(rule);
}

void te::qt::widgets::GroupingWidget::createStringNullGroupingItem(int count)
{
  if(count == 0)
    return;

  std::string attrName = m_ui->m_attrComboBox->currentText().toUtf8().data();

  std::string* ruleName = new std::string(NO_TITLE);

  te::se::Rule* rule = new te::se::Rule;
  rule->setName(ruleName);
  rule->setFilter(te::fe::CreateFilterByUniqueValue(attrName, te::common::Globals::sm_nanStr));

  int geomType = getGeometryType();
  std::vector<te::se::Symbolizer*> symbVec;
  te::se::Symbolizer* s = te::se::CreateSymbolizer((te::gm::GeomType)geomType, "#dddddd");
  symbVec.push_back(s);
  rule->setSymbolizers(symbVec);

  m_rules.push_back(rule);
}

int te::qt::widgets::GroupingWidget::getGeometryType()
{
  assert(m_layer.get());

  return te::map::GetGeomType(m_layer);
}

void te::qt::widgets::GroupingWidget::buildSymbolizer(std::string meanTitle)
{
  delete m_cb;

  m_cb = m_colorBar->getColorBar()->getColorBar();

  int legendSize = static_cast<int>(m_rules.size());

  std::vector<te::color::RGBAColor> colorVec;
  
  if(meanTitle.empty())
  {
    colorVec = m_cb->getSlices(legendSize);
  }
  else
  {
    int beforeMean = 0;
    int afterMean = 0;

    for(size_t t = 0; t < m_rules.size(); ++t)
    {
      std::string ruleName = *m_rules[t]->getName();
      if (ruleName != meanTitle)
      {
        beforeMean++;
      }
      else
      {
        afterMean = static_cast<int>(m_rules.size() - t - 1);
        break;
      }
    }

    std::vector<te::color::RGBAColor> lowerColorVec = m_cb->getLowerMeanSlices(beforeMean);
    te::color::RGBAColor meanColor = m_cb->getMeanSlice();
    std::vector<te::color::RGBAColor> upperColorVec = m_cb->getUpperMeanSlices(afterMean);

    for(size_t t = 0; t < lowerColorVec.size(); ++t)
      colorVec.push_back(lowerColorVec[t]);

    colorVec.push_back(meanColor);

    for(size_t t = 0; t < upperColorVec.size(); ++t)
      colorVec.push_back(upperColorVec[t]);
  }

  if (colorVec.size() != m_rules.size())
    return;

  int geomType = getGeometryType();

  for(size_t t = 0; t < colorVec.size(); ++t)
  {
    std::vector<te::se::Symbolizer*> symbVec;

    te::se::Symbolizer* s = te::se::CreateSymbolizer((te::gm::GeomType)geomType, colorVec[t].getColor());

    symbVec.push_back(s);

    m_rules[t]->setSymbolizers(symbVec);
  }
}

void te::qt::widgets::GroupingWidget::listAttributes()
{
  QString curValue = m_ui->m_attrComboBox->currentText();

  m_ui->m_attrComboBox->clear();

  std::unique_ptr<te::map::LayerSchema> dsType(m_layer->getSchema());

  //grouping type
  int index = m_ui->m_typeComboBox->currentIndex();
  int type = m_ui->m_typeComboBox->itemData(index).toInt();

  if(type == te::map::EQUAL_STEPS ||type == te::map::QUANTIL || type == te::map::STD_DEVIATION)
  {
    for(size_t t = 0; t < dsType->getProperties().size(); ++t)
    {
      te::dt::Property* p = dsType->getProperty(t);

      int dataType = p->getType();

      switch(dataType)
      {
        case te::dt::INT16_TYPE:
        case te::dt::INT32_TYPE:
        case te::dt::INT64_TYPE:
        case te::dt::FLOAT_TYPE:
        case te::dt::DOUBLE_TYPE:
        case te::dt::NUMERIC_TYPE:
          m_ui->m_attrComboBox->addItem(p->getName().c_str(), p->getType());

        default:
          continue;
      }
    }
  }
  else if(type == te::map::UNIQUE_VALUE) 
  {
    for(size_t t = 0; t < dsType->getProperties().size(); ++t)
    {
      te::dt::Property* p = dsType->getProperty(t);

      int dataType = p->getType();

      switch(dataType)
      {
        case te::dt::INT16_TYPE:
        case te::dt::INT32_TYPE:
        case te::dt::INT64_TYPE:
        case te::dt::FLOAT_TYPE:
        case te::dt::DOUBLE_TYPE:
        case te::dt::NUMERIC_TYPE:
        case te::dt::STRING_TYPE:
          m_ui->m_attrComboBox->addItem(p->getName().c_str(), p->getType());

        default:
          continue;
      }
    }
  }

  if(curValue.isEmpty() == false)
  {
    int idx = m_ui->m_attrComboBox->findText(curValue);

    if(idx != -1)
      m_ui->m_attrComboBox->setCurrentIndex(idx);
  }
}

void te::qt::widgets::GroupingWidget::setLayers(te::map::AbstractLayerPtr selectedLayer, std::vector<te::map::AbstractLayerPtr> allLayers)
{
  m_layer = selectedLayer;

  //set data type
  setDataSetType();

  //set grouping
  setGrouping();

  //Adjusting summary options
  m_ui->m_summaryComboBox->clear();
  if(te::da::HasLinkedTable(m_layer->getSchema().get()))
  {
    m_ui->m_summaryComboBox->addItem("MIN");
    m_ui->m_summaryComboBox->addItem("MAX");
    m_ui->m_summaryComboBox->addItem("SUM");
    m_ui->m_summaryComboBox->addItem("AVERAGE");
    m_ui->m_summaryComboBox->addItem("MEDIAN");
    m_ui->m_summaryComboBox->addItem("STDDEV");
    m_ui->m_summaryComboBox->addItem("VARIANCE");
    m_ui->m_summaryComboBox->addItem("NONE");

    if (m_layer->getGrouping())
    {
      int index = m_ui->m_summaryComboBox->findText(QString::fromUtf8(m_layer->getGrouping()->getSummary().c_str()));
      m_ui->m_summaryComboBox->setCurrentIndex(index);
    }

    m_ui->m_summaryComboBox->setEnabled(true);
    m_ui->m_summaryComboBox->show();
    m_ui->m_summaryLabel->show();
  }
  else
  {
    m_ui->m_summaryComboBox->addItem("NONE");
    m_ui->m_summaryComboBox->setEnabled(false);
    m_ui->m_summaryComboBox->hide();
    m_ui->m_summaryLabel->hide();
  }

  for(std::size_t i = 0; i < allLayers.size(); ++i)
  {
    if(!allLayers[i]->isValid())
      continue;

    std::unique_ptr<te::da::DataSetType> dt(allLayers[i]->getSchema());

    if(dt->hasGeom() && allLayers[i]->getGrouping() && allLayers[i]->getId() != selectedLayer->getId())
    {
      m_ui->m_layersComboBox->addItem(allLayers[i]->getTitle().c_str(), QVariant::fromValue(allLayers[i]));
    }
  }
}

void te::qt::widgets::GroupingWidget::onImportPushButtonClicked()
{
  if(m_ui->m_layersComboBox->currentText() == "")
  {
    QMessageBox::warning(this, tr("Grouping"), tr("There are no other layers with Grouping!"));
    return;
  }

  if(m_manual)
  {
    int reply = QMessageBox::question(this, tr("Grouping"), tr("Manual changes will be lost. Continue?"), QMessageBox::Yes | QMessageBox::Cancel);

    if(reply != QMessageBox::Yes)
      return;
  }

  QVariant varLayer = m_ui->m_layersComboBox->itemData(m_ui->m_layersComboBox->currentIndex(), Qt::UserRole);
  te::map::AbstractLayerPtr layer = varLayer.value<te::map::AbstractLayerPtr>();

  te::map::Grouping* ref = layer->getGrouping();

  std::unique_ptr<te::da::DataSetType> dt = m_layer->getSchema();

  std::vector<te::dt::Property*> props = dt->getProperties();

  bool isValid = false;
  for(std::size_t i = 0; i < props.size(); ++i)
  {
    te::dt::Property* prop = props[i];
    if((te::common::Convert2UCase(prop->getName()) == te::common::Convert2UCase(ref->getPropertyName())) && (prop->getType() == ref->getPropertyType()))
    {
      isValid = true;
      break;
    }
  }

  if(!isValid)
  {
    QString err = tr("There is no grouping that can be imported!\nThe layer must have an attribute with the same name of the attribute used to make the reference layer grouping: ");
    err.append(ref->getPropertyName().c_str());
    QMessageBox::warning(this, tr("Grouping"), err);
    return;
  }

  te::map::Grouping* newGrouping = new te::map::Grouping(*ref);

  setGrouping(newGrouping, layer->getStyle());

  updateUi(true);

  m_manual = false;

  emit applyPushButtonClicked();
}

void te::qt::widgets::GroupingWidget::onLoadPushButtonClicked()
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

      std::string attrName = legend.get<std::string>("Attribute");
      std::string precision = legend.get<std::string>("Precision");
      std::string catalog = legend.get<std::string>("Catalog");
      std::string group = legend.get<std::string>("Group");
      std::string schema = legend.get<std::string>("Schema");

      if(!m_ui->m_attrComboBox->findText(attrName.c_str()))
      {
        QMessageBox::warning(this, tr("Grouping"), tr("Attribute not found."));
        return;
      }

      m_ui->m_attrComboBox->setCurrentIndex(m_ui->m_attrComboBox->findText(attrName.c_str()));
      m_ui->m_precSpinBox->setValue(boost::lexical_cast<double>(precision));

      m_colorBar->loadDefaultColorCatalog();

      m_colorBar->setCatalog(catalog);
      m_colorBar->setGroup(group);
      m_colorBar->setSchema(schema);

      std::vector<std::vector<std::string> > items;

      for(boost::property_tree::ptree::value_type &v: legend.get_child("Items"))
      {
        std::vector<std::string> item;
        item.push_back(v.second.get<std::string>("Title"));
        item.push_back(v.second.get<std::string>("Min"));
        item.push_back(v.second.get<std::string>("Max"));
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
        m_ui->m_tableWidget->setItem(i, 3, new QTableWidgetItem(items[i][2].c_str()));

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

void te::qt::widgets::GroupingWidget::onSavePushButtonClicked()
{
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save as..."),
    QString(), tr("LEG (*.leg *.LEG);;"), nullptr, QFileDialog::DontConfirmOverwrite);

  saveLegend(fileName.toUtf8().data());
}

void te::qt::widgets::GroupingWidget::saveLegend(const std::string& path)
{
  int rowCount = m_ui->m_tableWidget->rowCount();

  if (rowCount < 1)
    return;

  if (path.empty())
    return;

  boost::property_tree::ptree pt;
  boost::property_tree::ptree legend;

  std::string attrName = m_ui->m_attrComboBox->currentText().toUtf8().data();
  std::string precision = m_ui->m_precSpinBox->text().toUtf8().data();
  std::string catalog = m_colorBar->getCatalog();
  std::string group = m_colorBar->getGroup();
  std::string schema = m_colorBar->getSchema();

  legend.add("Attribute", attrName);
  legend.add("Precision", precision);
  legend.add("Catalog", catalog);
  legend.add("Group", group);
  legend.add("Schema", schema);

  boost::property_tree::ptree items;

  for (int i = 0; i < m_ui->m_tableWidget->rowCount(); ++i)
  {
    std::string title = m_ui->m_tableWidget->item(i, 1)->text().toUtf8().data();
    std::string min = m_ui->m_tableWidget->item(i, 2)->text().toUtf8().data();
    std::string max = m_ui->m_tableWidget->item(i, 3)->text().toUtf8().data();

    te::se::Rule* rule = m_rules[i];

    const std::vector<te::se::Symbolizer*>& ss = rule->getSymbolizers();
    te::se::PolygonSymbolizer* ps = (te::se::PolygonSymbolizer*)ss[0];
    const te::se::Fill* fill = ps->getFill();
    te::color::RGBAColor color = te::se::GetColor(fill->getColor());

    boost::property_tree::ptree item;
    item.add("Title", title);
    item.add("Min", min);
    item.add("Max", max);
    item.add("R", color.getRed());
    item.add("G", color.getGreen());
    item.add("B", color.getBlue());

    items.add_child("Item", item);
  }

  legend.add_child("Items", items);

  pt.add_child("Legend", legend);

  boost::property_tree::write_json(path, pt);
}
