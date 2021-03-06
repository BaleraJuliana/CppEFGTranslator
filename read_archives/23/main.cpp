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
\file terralib/mnt/qt/VolumeDialog.cpp

\brief A dialog for Calculate Volume
*/

//terralib
#include "../../common/Exception.h"
#include "../../common/progress/ProgressManager.h"
#include "../../core/translator/Translator.h"
#include "../../dataaccess/datasource/DataSourceFactory.h"
#include "../../dataaccess/datasource/DataSourceInfoManager.h"
#include "../../dataaccess/datasource/DataSourceManager.h"
#include "../../dataaccess/utils/Utils.h"
#include "../../geometry/GeometryProperty.h"
#include "../../maptools/DataSetLayer.h"
#include "../../maptools/Utils.h"
#include "../../mnt/core/Utils.h"
#include "../../mnt/core/Volume.h"
#include "../../qt/af/ApplicationController.h"
#include "../../qt/widgets/datasource/selector/DataSourceSelectorDialog.h"
#include "../../qt/widgets/progress/ProgressViewerDialog.h"
#include "../../qt/widgets/rp/Utils.h"
#include "../../raster.h"


#include "LayerSearchDialog.h"
#include "VolumeDialog.h"
#include "VolumeResultDialog.h"
#include "ui/ui_VolumeDialogForm.h"

// Qt
#include <QFileDialog>
#include <QMessageBox>

te::mnt::VolumeDialog::VolumeDialog(QWidget* parent, Qt::WindowFlags f)
  : QDialog(parent, f),
  m_ui(new Ui::VolumeDialogForm),
  m_layers(std::list<te::map::AbstractLayerPtr>())
{
  // add controls
  m_ui->setupUi(this);

  //signals
  connect(m_ui->m_rasterSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputRasterToolButtonClicked()));
  connect(m_ui->m_rasterlayersComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onRasterInputComboBoxChanged(int)));

  connect(m_ui->m_dummycheckBox, SIGNAL(toggled(bool)), m_ui->m_dummylineEdit, SLOT(setEnabled(bool)));
  connect(m_ui->m_dummylineEdit, SIGNAL(editingFinished()), this, SLOT(onDummyLineEditEditingFinished()));

  connect(m_ui->m_vectorSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputVectorToolButtonClicked()));
  connect(m_ui->m_vectorlayersComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onVectorInputComboBoxChanged(int)));

  connect(m_ui->m_okPushButton, SIGNAL(clicked()), this, SLOT(onOkPushButtonClicked()));
  connect(m_ui->m_cancelPushButton, SIGNAL(clicked()), this, SLOT(onCancelPushButtonClicked()));

  m_ui->m_helpPushButton->setNameSpace("dpi.inpe.br.plugins");
  m_ui->m_helpPushButton->setPageReference("plugins/mnt/DTM_Volume.html");

  setModal(false);

}

te::mnt::VolumeDialog::~VolumeDialog() = default;

void te::mnt::VolumeDialog::setLayers(std::list<te::map::AbstractLayerPtr> layers)
{
  m_layers = layers;

  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();

  while (it != m_layers.end())
  {
    if (it->get())
    {
      if (it->get()->isValid())
      {
        std::unique_ptr<te::da::DataSetType> dsType(it->get()->getSchema());
        mntType type = getMNTType(dsType.get());

        if (type == TIN)
          m_ui->m_vectorlayersComboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));

        if (type == GRID)
            m_ui->m_rasterlayersComboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));

        dsType.release();
      }
    }
    ++it;
  }
}


void te::mnt::VolumeDialog::release()
{
}

void te::mnt::VolumeDialog::onInputRasterToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(GRID);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }
  
  int index = m_ui->m_rasterlayersComboBox->findText(search.getLayer().get()->getTitle().c_str());
  m_ui->m_rasterlayersComboBox->setCurrentIndex(index);

}

void te::mnt::VolumeDialog::onRasterInputComboBoxChanged(int index)
{
  m_rasterinputLayer = nullptr;
  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_rasterlayersComboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();
  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_rasterinputLayer = it->get();
      //std::unique_ptr<te::da::DataSetType> dsType = m_rasterinputLayer->getSchema();
      //te::rst::RasterProperty* rasterProp = te::da::GetFirstRasterProperty(dsType.get());
      //te::map::DataSetLayer* indsLayer = dynamic_cast<te::map::DataSetLayer*>(m_rasterinputLayer.get());
      //te::da::DataSourcePtr inDataSource = te::da::GetDataSource(indsLayer->getDataSourceId(), true);
      //std::unique_ptr<te::da::DataSet> dsRaster = inDataSource->getDataSet(indsLayer->getDataSetName());
      //std::unique_ptr<te::rst::Raster> in_raster = dsRaster->getRaster(rasterProp->getName());
      std::unique_ptr<te::rst::Raster> in_raster(te::map::GetRaster(m_rasterinputLayer.get()));
      m_ui->m_dummylineEdit->setText(QString::number(in_raster->getBand(0)->getProperty()->m_noDataValue));
     // in_raster.release();
      //dsRaster.release();
     // dsType.release();
    }
    ++it;
  }
}

void te::mnt::VolumeDialog::onDummyLineEditEditingFinished()
{
}

void te::mnt::VolumeDialog::onInputVectorToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget(), nullptr, false);
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(TIN);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }

  int index = m_ui->m_vectorlayersComboBox->findText(search.getLayer().get()->getTitle().c_str());
  m_ui->m_vectorlayersComboBox->setCurrentIndex(index);
}

void te::mnt::VolumeDialog::onVectorInputComboBoxChanged(int index)
{
  m_vectorinputLayer = nullptr;
  m_ui->m_attributeComboBox->clear();
  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_vectorlayersComboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();
  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_vectorinputLayer = it->get();

      std::unique_ptr<te::da::DataSetType> dsType = m_vectorinputLayer->getSchema();
      std::vector<te::dt::Property*> props = dsType->getProperties();
      for (std::size_t i = 0; i < props.size(); ++i)
      {
        m_ui->m_attributeComboBox->addItem(QString(props[i]->getName().c_str()), QVariant(props[i]->getName().c_str()));
      }
      dsType.release();
    }
    ++it;
  }
}

void te::mnt::VolumeDialog::onOkPushButtonClicked()
{
  //progress
  te::qt::widgets::ProgressViewerDialog v(this);

  try
  {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (!m_rasterinputLayer.get() || !m_vectorinputLayer.get())
      throw te::common::Exception(TE_TR("Select an input layer!"));

    if (m_rasterinputLayer->getSRID() != m_vectorinputLayer->getSRID())
      throw te::common::Exception(TE_TR("Can not execute this operation with different SRIDs geometries!"));

    te::map::DataSetLayer* indsrasterLayer = dynamic_cast<te::map::DataSetLayer*>(m_rasterinputLayer.get());
    if (!indsrasterLayer)
      throw te::common::Exception(TE_TR("Can not execute this operation on this type of layer!"));

    te::da::DataSourcePtr inrasterDataSource = te::da::GetDataSource(indsrasterLayer->getDataSourceId(), true);
    if (!inrasterDataSource.get())
      throw te::common::Exception(TE_TR("The selected input data source can not be accessed!"));

    std::string inrasterDsetName = indsrasterLayer->getDataSetName();
    std::unique_ptr<te::da::DataSetType> inrasterDsetType(inrasterDataSource->getDataSetType(inrasterDsetName));

    const te::da::ObjectIdSet* OidSet = nullptr;
    if (m_ui->m_selectCheckBox->isChecked())
    {
      OidSet = m_vectorinputLayer->getSelected();
      if (!OidSet)
        throw te::common::Exception(TE_TR("Select the layer objects to perform the intersection operation."));
    }

    te::map::DataSetLayer* indsvectorLayer = dynamic_cast<te::map::DataSetLayer*>(m_vectorinputLayer.get());
    if (!indsvectorLayer)
      throw te::common::Exception(TE_TR("Can not execute this operation on this type of layer!"));

    te::da::DataSourcePtr invectorDataSource = te::da::GetDataSource(indsvectorLayer->getDataSourceId(), true);
    if (!invectorDataSource.get())
      throw te::common::Exception(TE_TR("The selected input data source can not be accessed!"));

    std::string invectorDsetName = indsvectorLayer->getDataSetName();
    std::unique_ptr<te::da::DataSetType> invectorDsetType(invectorDataSource->getDataSetType(invectorDsetName));

    Volume *calvol = new te::mnt::Volume();

    std::string attr = m_ui->m_attributeComboBox->currentText().toUtf8().data();

    int srid = m_vectorinputLayer->getSRID();
    calvol->setSRID(srid);
    calvol->setInput(inrasterDataSource, inrasterDsetName, std::move(inrasterDsetType), invectorDataSource, invectorDsetName, std::move(invectorDsetType), OidSet);
    calvol->setParams(m_ui->m_quotaLineEdit->text().toDouble(), attr, m_ui->m_dummylineEdit->text().toDouble());

    if (calvol->run())
    {
      QApplication::restoreOverrideCursor();
      std::vector<std::string> polyvec;
      std::vector<std::string> cortevec;
      std::vector<std::string> aterrovec;
      std::vector<std::string> areavec;
      std::vector<std::string> iquotavec;
      calvol->getResults(polyvec, cortevec, aterrovec, areavec, iquotavec);

      te::mnt::VolumeResultDialog result(polyvec,
        cortevec,
        aterrovec,
        areavec,
        iquotavec,
        attr,
        this->parentWidget());

      if (result.exec() != QDialog::Accepted)
      {
        return;
      }
    }
    delete calvol;
  }
  catch (const std::exception& e)
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, tr("Volume "), e.what());
    return;
  }

  QApplication::restoreOverrideCursor();
  accept();
}

void te::mnt::VolumeDialog::onCancelPushButtonClicked()
{
  reject();
}
