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
  \file terralib/sa/qt/KernelMapDialog.cpp

  \brief A dialog to calculate the kernel map of a dataset.
*/

// TerraLib
#include "../../core/logger/Logger.h"
#include "../../common/progress/ProgressManager.h"
#include "../../core/translator/Translator.h"
#include "../../common/STLUtils.h"
#include "../../dataaccess/datasource/DataSource.h"
#include "../../dataaccess/utils/Utils.h"
#include "../../geometry/GeometryProperty.h"
#include "../../maptools/DataSetLayer.h"
#include "../../qt/widgets/progress/ProgressViewerDialog.h"
#include "../core/KernelMapOperation.h"
#include "../Exception.h"
#include "KernelMapDialog.h"
#include "Utils.h"
#include "ui_KernelMapDialogForm.h"

// Qt
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QValidator>

// STL
#include <memory>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

Q_DECLARE_METATYPE(te::map::AbstractLayerPtr)

te::sa::KernelMapDialog::KernelMapDialog(QWidget* parent, Qt::WindowFlags f)
  : QDialog(parent, f),
    m_ui(new Ui::KernelMapDialogForm)
{
// add controls
  m_ui->setupUi(this);

  m_ui->m_nColsLineEdit->setValidator(new QIntValidator(this));

  fillKernelParameters();

// add icons
  m_ui->m_imgLabel->setPixmap(QIcon::fromTheme("sa-kernelmap-hint").pixmap(112,48));

// connectors
  connect(m_ui->m_inputLayerComboBox, SIGNAL(activated(int)), this, SLOT(onInputLayerComboBoxActivated(int)));
  connect(m_ui->m_okPushButton, SIGNAL(clicked()), this, SLOT(onOkPushButtonClicked()));
  connect(m_ui->m_targetFileToolButton, SIGNAL(pressed()), this,  SLOT(onTargetFileToolButtonPressed()));

// help info
  m_ui->m_helpPushButton->setNameSpace("dpi.inpe.br.plugins"); 
  m_ui->m_helpPushButton->setPageReference("plugins/sa/sa_kernelmap.html");
}

te::sa::KernelMapDialog::~KernelMapDialog() = default;

void te::sa::KernelMapDialog::setLayers(std::list<te::map::AbstractLayerPtr> layers)
{
  std::list<te::map::AbstractLayerPtr>::iterator it = layers.begin();

  while(it != layers.end())
  {
    te::map::AbstractLayerPtr l = *it;

    if(l->isValid())
    {
      std::unique_ptr<te::da::DataSetType> dsType = l->getSchema();

      te::map::DataSetLayer* dsLayer = dynamic_cast<te::map::DataSetLayer*>(l.get());

      if(dsLayer && dsType->hasGeom())
        m_ui->m_inputLayerComboBox->addItem(it->get()->getTitle().c_str(), QVariant::fromValue(l));
    }

    ++it;
  }

// fill attributes combo
  if(m_ui->m_inputLayerComboBox->count() > 0)
    onInputLayerComboBoxActivated(0);
}

te::map::AbstractLayerPtr te::sa::KernelMapDialog::getOutputLayer()
{
  return m_outputLayer;
}

void te::sa::KernelMapDialog::fillKernelParameters()
{
  //function
  m_ui->m_functionComboBox->clear();
  
  m_ui->m_functionComboBox->addItem("Quartic", QVariant(te::sa::Quartic));
  m_ui->m_functionComboBox->addItem("Normal", QVariant(te::sa::Normal));
  m_ui->m_functionComboBox->addItem("Triangular", QVariant(te::sa::Triangular));
  m_ui->m_functionComboBox->addItem("Uniform", QVariant(te::sa::Uniform));
  m_ui->m_functionComboBox->addItem("Negative Exponential", QVariant(te::sa::Negative_Exp));

  //estimation
  m_ui->m_estimationComboBox->clear();
  
  m_ui->m_estimationComboBox->addItem("Density", QVariant(te::sa::Density));
  m_ui->m_estimationComboBox->addItem("Spatial Moving Average", QVariant(te::sa::Spatial_Moving_Average));
  m_ui->m_estimationComboBox->addItem("Probability", QVariant(te::sa::Probability));
}

void te::sa::KernelMapDialog::onInputLayerComboBoxActivated(int index)
{
  QVariant varLayer = m_ui->m_inputLayerComboBox->itemData(index, Qt::UserRole);
  
  te::map::AbstractLayerPtr l = varLayer.value<te::map::AbstractLayerPtr>();

  std::unique_ptr<te::da::DataSetType> dsType = l->getSchema();

  std::vector<te::dt::Property*> propVec = dsType->getProperties();

  m_ui->m_attrLayerComboBox->clear();

  for(std::size_t t = 0; t < propVec.size(); ++t)
  {
    int dataType = propVec[t]->getType();

    if (dataType == te::dt::INT16_TYPE || dataType == te::dt::UINT16_TYPE ||
        dataType == te::dt::INT32_TYPE || dataType == te::dt::UINT32_TYPE ||
        dataType == te::dt::INT64_TYPE || dataType == te::dt::UINT64_TYPE ||
        dataType == te::dt::FLOAT_TYPE || dataType == te::dt::DOUBLE_TYPE)
    {
      m_ui->m_attrLayerComboBox->addItem(propVec[t]->getName().c_str(), dataType);
    }
  }

  //output data restriction for point representation
  te::gm::GeometryProperty* geomProp = te::da::GetFirstGeomProperty(dsType.get());

  if (geomProp)
  {
    if (geomProp->getGeometryType() == te::gm::PointType ||
        geomProp->getGeometryType() == te::gm::PointZType ||
        geomProp->getGeometryType() == te::gm::PointMType ||
        geomProp->getGeometryType() == te::gm::PointZMType ||
        geomProp->getGeometryType() == te::gm::PointKdType ||
        geomProp->getGeometryType() == te::gm::MultiPointType ||
        geomProp->getGeometryType() == te::gm::MultiPointZType ||
        geomProp->getGeometryType() == te::gm::MultiPointMType ||
        geomProp->getGeometryType() == te::gm::MultiPointZMType)
    {
      m_ui->m_gridRadioButton->setChecked(true);
      m_ui->m_attrRadioButton->setEnabled(false);
      m_ui->m_attrNameLineEdit->clear();
    }
    else
    {
      m_ui->m_gridRadioButton->setChecked(true);
      m_ui->m_attrRadioButton->setEnabled(true);
      m_ui->m_attrNameLineEdit->clear();
    }
  }
}


void te::sa::KernelMapDialog::onOkPushButtonClicked()
{
  // check input parameters
  if(m_ui->m_repositoryLineEdit->text().isEmpty())
  {
    QMessageBox::information(this, tr("Warning"), tr("Define a repository for the result."));
    return;
  }

  if(m_ui->m_newLayerNameLineEdit->text().isEmpty())
  {
    QMessageBox::information(this, tr("Warning"), tr("Define a name for the resulting layer."));
    return;
  }

  //get selected layer
  QVariant varLayer = m_ui->m_inputLayerComboBox->itemData(m_ui->m_inputLayerComboBox->currentIndex(), Qt::UserRole);
  te::map::AbstractLayerPtr l = varLayer.value<te::map::AbstractLayerPtr>();

  //set input kernel parameters
  te::sa::KernelInputParams* kInParams = new te::sa::KernelInputParams();

  kInParams->m_functionType = (te::sa::KernelFunctionType) m_ui->m_functionComboBox->itemData(m_ui->m_functionComboBox->currentIndex()).toInt();
  kInParams->m_estimationType = (te::sa::KernelEstimationType) m_ui->m_estimationComboBox->itemData(m_ui->m_estimationComboBox->currentIndex()).toInt();

  kInParams->m_useAdaptativeRadius = m_ui->m_useAdaptRadiusCheckBox->isChecked();
  
  if(!m_ui->m_useAdaptRadiusCheckBox->isChecked())
  {
    kInParams->m_radiusPercentValue = m_ui->m_radiusSpinBox->value();
  }

  if(m_ui->m_useAttrLayerCheckBox->isChecked())
  {
    kInParams->m_intensityAttrName = m_ui->m_attrLayerComboBox->currentText().toUtf8().data();
  }

  kInParams->m_ds = l->getData();
  kInParams->m_dsType = l->getSchema();

  //set output kernel parameters
  te::sa::KernelOutputParams* kOutParams = new te::sa::KernelOutputParams();

  kOutParams->m_outputPath = m_ui->m_repositoryLineEdit->text().toUtf8().data();
  kOutParams->m_outputDataSetName = m_ui->m_newLayerNameLineEdit->text().toUtf8().data();

  if(m_ui->m_gridRadioButton->isChecked())
  {
    if(m_ui->m_nColsLineEdit->text().isEmpty())
    {
      QMessageBox::information(this, tr("Warning"), tr("Define the number of columns to the output grid."));
      return;
    }

    kOutParams->m_storageType = te::sa::Grid;
    kOutParams->m_nCols = m_ui->m_nColsLineEdit->text().toInt();
  }
  else if(m_ui->m_attrRadioButton->isChecked())
  {
    if(m_ui->m_attrNameLineEdit->text().isEmpty())
    {
      QMessageBox::information(this, tr("Warning"), tr("Define the attribute name to the output data."));
      return;
    }

    kOutParams->m_storageType = te::sa::Attribute;
    kOutParams->m_outputAttrName = m_ui->m_attrNameLineEdit->text().toUtf8().data();
  }

  //progress
  te::qt::widgets::ProgressViewerDialog v(this);

  QApplication::setOverrideCursor(Qt::WaitCursor);

  //execute kernel
  try
  {
    te::sa::KernelMapOperation op;

    op.setInputParameters(kInParams);
    op.setOutputParameters(kOutParams);

    op.execute();
  }
  catch(const std::exception& e)
  {
    QMessageBox::warning(this, tr("Warning"), e.what());

    QApplication::restoreOverrideCursor();

    return;
  }
  catch(...)
  {
    QMessageBox::warning(this, tr("Warning"), tr("Kernel Map internal error."));

    QApplication::restoreOverrideCursor();

    return;
  }

  QApplication::restoreOverrideCursor();

  //save generated information
  te::da::DataSourcePtr outputDataSource;

  std::string dataSetName = "";

  if(m_ui->m_gridRadioButton->isChecked())
  {
    outputDataSource = te::sa::CreateGDALDataSource(m_ui->m_repositoryLineEdit->text().toUtf8().data(), m_ui->m_newLayerNameLineEdit->text().toUtf8().data());

    dataSetName = std::string(m_ui->m_newLayerNameLineEdit->text().toUtf8().data()) + ".tif";
  }
  else if(m_ui->m_attrRadioButton->isChecked())
  {
    outputDataSource = te::sa::CreateOGRDataSource(m_ui->m_repositoryLineEdit->text().toUtf8().data(), m_ui->m_newLayerNameLineEdit->text().toUtf8().data());

    dataSetName = m_ui->m_newLayerNameLineEdit->text().toUtf8().data();
  }

  //create layer
  m_outputLayer = te::sa::CreateLayer(outputDataSource, dataSetName);

  //create legend
  if(m_ui->m_gridRadioButton->isChecked())
  {
    te::sa::CreateKernelColorMaping(m_outputLayer);
  }
  else if(m_ui->m_attrRadioButton->isChecked())
  {
    te::sa::CreateKernelGrouping(m_outputLayer, m_ui->m_attrNameLineEdit->text().toUtf8().data());
  }

  accept();
}

void te::sa::KernelMapDialog::onTargetFileToolButtonPressed()
{
  m_ui->m_newLayerNameLineEdit->clear();
  m_ui->m_repositoryLineEdit->clear();
  
  QString fileName = QFileDialog::getExistingDirectory(this, tr("Set output location..."));
  
  if (fileName.isEmpty())
    return;

  m_ui->m_repositoryLineEdit->setText(fileName);
}
