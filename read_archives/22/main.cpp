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
\file terralib/mnt/qt/TINGenerationDialog.cpp

\brief A dialog for TIN generation
*/

//terralib
#include "../../core/filesystem/FileSystem.h"
#include "../../core/translator/Translator.h"
#include "../../common/Exception.h"
#include "../../common/progress/ProgressManager.h"
#include "../../common/UnitsOfMeasureManager.h"
#include "../../dataaccess/datasource/DataSourceFactory.h"
#include "../../dataaccess/datasource/DataSourceInfoManager.h"
#include "../../dataaccess/datasource/DataSourceManager.h"
#include "../../dataaccess/utils/Utils.h"
#include "../../geometry/GeometryProperty.h"
#include "../../maptools/DataSetLayer.h"
#include "../../mnt/core/TINGeneration.h"
#include "../../mnt/core/Utils.h"
#include "../../qt/widgets/progress/ProgressViewerDialog.h"
#include "../../qt/widgets/datasource/selector/DataSourceSelectorDialog.h"
#include "../../qt/widgets/layer/utils/DataSet2Layer.h"
#include "../../qt/widgets/srs/SRSManagerDialog.h"
#include "../../qt/widgets/utils/FileDialog.h"
#include "../../srs/SpatialReferenceSystemManager.h"

#include "LayerSearchDialog.h"
#include "TINGenerationDialog.h"
#include "ui/ui_TINGenerationDialogForm.h"

// Qt
#include <QFileDialog>
#include <QMessageBox>

// BOOST
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

te::mnt::TINGenerationDialog::TINGenerationDialog(QWidget* parent, Qt::WindowFlags f)
  : QDialog(parent, f),
  m_ui(new Ui::TINGenerationDialogForm),
  m_layers(std::list<te::map::AbstractLayerPtr>())
{
  // add controls
  m_ui->setupUi(this);

  //signals
  connect(m_ui->m_isolinesSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputIsolinesToolButtonClicked()));
  connect(m_ui->m_isolinescomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onIsolinesComboBoxChanged(int)));
  connect(m_ui->m_sampleSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputSamplesToolButtonClicked()));
  connect(m_ui->m_samplescomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onSamplesComboBoxChanged(int)));

  connect(m_ui->m_scalepushButton, SIGNAL(clicked()), this, SLOT(onScalePushButtonClicked()));

  connect(m_ui->m_yesradioButton, SIGNAL(toggled(bool)), this, SLOT(onYesToggled()));
  connect(m_ui->m_noradioButton, SIGNAL(toggled(bool)), this, SLOT(onNoToggled()));
  connect(m_ui->m_breaklinecomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onBreakLinesComboBoxChanged(int)));
  connect(m_ui->m_breaklineSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputBreaklineToolButtonClicked()));

  m_ui->m_targetDatasourceToolButton->setIcon(QIcon::fromTheme("datasource"));
  connect(m_ui->m_targetDatasourceToolButton, SIGNAL(pressed()), this, SLOT(onTargetDatasourceToolButtonPressed()));
  connect(m_ui->m_targetFileToolButton, SIGNAL(pressed()), this, SLOT(onTargetFileToolButtonPressed()));

  connect(m_ui->m_okPushButton, SIGNAL(clicked()), this, SLOT(onOkPushButtonClicked()));
  connect(m_ui->m_cancelPushButton, SIGNAL(clicked()), this, SLOT(onCancelPushButtonClicked()));

  m_ui->m_helpPushButton->setNameSpace("dpi.inpe.br.plugins");
  m_ui->m_helpPushButton->setPageReference("plugins/mnt/DTM_TIN.html");

  m_ui->m_srsToolButton->setIcon(QIcon::fromTheme("srs"));
  connect(m_ui->m_srsToolButton, SIGNAL(clicked()), this, SLOT(onSrsToolButtonClicked()));

  m_ui->m_noradioButton->setChecked(true);
  m_ui->m_isolinescomboBox->addItem(QString(""), QVariant(""));
  m_ui->m_samplescomboBox->addItem(QString(""), QVariant(""));
  m_ui->m_breaklinecomboBox->addItem(QString(""), QVariant(""));

  m_isosrid = 0;
  m_samplesrid = 0;
  m_outsrid = 0;


}

te::mnt::TINGenerationDialog::~TINGenerationDialog() = default;

void te::mnt::TINGenerationDialog::setLayers(std::list<te::map::AbstractLayerPtr> layers)
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

        if (type == SAMPLE)
          m_ui->m_samplescomboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));

        if (type == ISOLINE)
        {
          m_ui->m_isolinescomboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));
          m_ui->m_breaklinecomboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));
        }
      }
    }
    ++it;
  }
}

void te::mnt::TINGenerationDialog::onInputIsolinesToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(ISOLINE);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }

  int index = m_ui->m_isolinescomboBox->findText(search.getLayer().get()->getTitle().c_str());
  m_ui->m_isolinescomboBox->setCurrentIndex(index);
}

void te::mnt::TINGenerationDialog::onIsolinesComboBoxChanged(int index)
{
  m_ui->m_isolinesZcomboBox->clear();
  m_isolinesLayer = nullptr;
  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_isolinescomboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();
  try{
    while (it != m_layers.end())
    {
      if(layerID == it->get()->getId())
      {
        m_isolinesLayer = it->get();
        std::unique_ptr<te::da::DataSetType> dsType = it->get()->getSchema();

        te::map::DataSetLayer* dsisoLayer = dynamic_cast<te::map::DataSetLayer*>(m_isolinesLayer.get());
        if (!dsisoLayer)
          throw te::common::Exception(TE_TR("Can not execute this operation on this type of layer."));

        m_isolinesDataSource = te::da::GetDataSource(dsisoLayer->getDataSourceId(), true);
        if (!m_isolinesDataSource.get())
          throw te::common::Exception(TE_TR("The selected input data source can not be accessed."));

        m_isosrid = dsisoLayer->getSRID();

        setSRID(m_isosrid);

        m_isoSetName = dsisoLayer->getDataSetName();

        std::unique_ptr<te::da::DataSet> inDset = m_isolinesDataSource->getDataSet(m_isoSetName);
        std::size_t geo_pos = te::da::GetFirstPropertyPos(inDset.get(), te::dt::GEOMETRY_TYPE);
        inDset->moveFirst();
        std::unique_ptr<te::gm::Geometry> gin = inDset->getGeometry(geo_pos);
        if (gin->is3D())
        {
          m_ui->m_isolinesZlabel->hide();
          m_ui->m_isolinesZcomboBox->hide();
          return;
        }

        m_ui->m_isolinesZlabel->show();
        m_ui->m_isolinesZcomboBox->show();

        std::vector<te::dt::Property*> props = dsType->getProperties();
        for (std::size_t i = 0; i < props.size(); ++i)
        {
          switch (props[i]->getType())
          {
          case te::dt::FLOAT_TYPE:
          case te::dt::DOUBLE_TYPE:
          case te::dt::INT16_TYPE:
          case te::dt::INT32_TYPE:
          case te::dt::INT64_TYPE:
          case te::dt::UINT16_TYPE:
          case te::dt::UINT32_TYPE:
          case te::dt::UINT64_TYPE:
          case te::dt::NUMERIC_TYPE:
            m_ui->m_isolinesZcomboBox->addItem(QString(props[i]->getName().c_str()), QVariant(props[i]->getName().c_str()));
            break;
          }
        }
        break;
      }
      it++;
    }
  }
  catch (const std::exception& e)
  {
    QMessageBox::information(this, tr("TIN Generation"), e.what());
    return;
  }
}

void te::mnt::TINGenerationDialog::onInputSamplesToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(SAMPLE);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }

  int index = m_ui->m_samplescomboBox->findText(search.getLayer().get()->getTitle().c_str());
  m_ui->m_samplescomboBox->setCurrentIndex(index);
}

void te::mnt::TINGenerationDialog::onSamplesComboBoxChanged(int index)
{
  try{
    m_ui->m_samplesZcomboBox->clear();
    m_samplesLayer = nullptr;
    std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
    std::string layerID = m_ui->m_samplescomboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();
    while (it != m_layers.end())
    {
      if(layerID == it->get()->getId())
      {
        m_samplesLayer = it->get();

        te::map::DataSetLayer* dssampleLayer = dynamic_cast<te::map::DataSetLayer*>(m_samplesLayer.get());
        if (!dssampleLayer)
          throw te::common::Exception(TE_TR("Can not execute this operation on this type of layer."));

        m_samplesDataSource = te::da::GetDataSource(dssampleLayer->getDataSourceId(), true);
        if (!m_samplesDataSource.get())
          throw te::common::Exception(TE_TR("The selected input data source can not be accessed."));

        m_sampleSetName = dssampleLayer->getDataSetName();
        m_samplesrid = dssampleLayer->getSRID();
        setSRID(m_samplesrid);

        std::unique_ptr<te::da::DataSet> inDset = m_samplesDataSource->getDataSet(m_sampleSetName);
        std::size_t geo_pos = te::da::GetFirstPropertyPos(inDset.get(), te::dt::GEOMETRY_TYPE);
        inDset->moveFirst();
        std::unique_ptr<te::gm::Geometry> gin = inDset->getGeometry(geo_pos);
        if (gin->is3D())
        {
          m_ui->m_samplesZlabel->hide();
          m_ui->m_samplesZcomboBox->hide();
          return;
        }

        m_ui->m_samplesZlabel->show();
        m_ui->m_samplesZcomboBox->show();

        std::unique_ptr<te::da::DataSetType> dsType = m_samplesLayer->getSchema();
        std::vector<te::dt::Property*> props = dsType->getProperties();

        for (std::size_t i = 0; i < props.size(); ++i)
        {
          switch (props[i]->getType())
          {
          case te::dt::FLOAT_TYPE:
          case te::dt::DOUBLE_TYPE:
          case te::dt::INT16_TYPE:
          case te::dt::INT32_TYPE:
          case te::dt::INT64_TYPE:
          case te::dt::UINT16_TYPE:
          case te::dt::UINT32_TYPE:
          case te::dt::UINT64_TYPE:
          case te::dt::NUMERIC_TYPE:
            m_ui->m_samplesZcomboBox->addItem(QString(props[i]->getName().c_str()), QVariant(props[i]->getName().c_str()));
            break;
          }
        }
      }
      it++;
    }
  }
  catch (const std::exception& e)
  {
    QMessageBox::information(this, tr("TIN Generation"), e.what());
    return;
  }
}

void te::mnt::TINGenerationDialog::onScalePushButtonClicked()
{
  m_scale = m_ui->m_scalelineEdit->text().toDouble();
  m_breaktol = m_tol = (m_scale * 0.4) / 1000;
  m_distance = m_tol * 20;
  m_edgeSize = m_tol / 5;

  m_ui->m_minedgelineEdit->setText(QString::number(m_edgeSize));
  m_ui->m_tollineEdit->setText(QString::number(m_tol));
  m_ui->m_breaktollineEdit->setText(QString::number(m_breaktol));
  m_ui->m_distancelineEdit->setText(QString::number(m_distance));

}

void te::mnt::TINGenerationDialog::onYesToggled()
{
  m_ui->m_breaklinelabel->setEnabled(true);
  m_ui->m_breaklinecomboBox->setEnabled(true);
  m_ui->m_breaktollabel->setEnabled(true);
  m_ui->m_breaktollineEdit->setEnabled(true);
}

void te::mnt::TINGenerationDialog::onNoToggled()
{
  m_ui->m_breaklinecomboBox->setCurrentIndex(0);
  m_ui->m_breaklinelabel->setDisabled(true);
  m_ui->m_breaklinecomboBox->setDisabled(true);
  m_ui->m_breaktollabel->setDisabled(true);
  m_ui->m_breaktollineEdit->setDisabled(true);
}

void te::mnt::TINGenerationDialog::onBreakLinesComboBoxChanged(int index)
{
  m_breaklinesLayer = nullptr;
  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_breaklinecomboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();
  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_breaklinesLayer = it->get();
      break;
    }
    it++;
  }
}

void te::mnt::TINGenerationDialog::onInputBreaklineToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget(), nullptr, false);
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(ISOLINE);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }

  int index = m_ui->m_breaklinecomboBox->findText(search.getLayer().get()->getTitle().c_str());
  m_ui->m_breaklinecomboBox->setCurrentIndex(index);
}

void te::mnt::TINGenerationDialog::onTargetDatasourceToolButtonPressed()
{
  m_ui->m_newLayerNameLineEdit->clear();
  m_ui->m_newLayerNameLineEdit->setEnabled(true);
  te::qt::widgets::DataSourceSelectorDialog dlg(this);
  dlg.exec();

  std::list<te::da::DataSourceInfoPtr> dsPtrList = dlg.getSelecteds();

  if (dsPtrList.empty())
    return;

  std::list<te::da::DataSourceInfoPtr>::iterator it = dsPtrList.begin();

  m_ui->m_repositoryLineEdit->setText(QString(it->get()->getTitle().c_str()));

  m_outputDatasource = *it;

  m_toFile = false;
}

void te::mnt::TINGenerationDialog::onTargetFileToolButtonPressed()
{
  m_ui->m_newLayerNameLineEdit->clear();
  m_ui->m_repositoryLineEdit->clear();

  te::qt::widgets::FileDialog fileDialog(this, te::qt::widgets::FileDialog::VECTOR);

  try {
    fileDialog.exec();
  }
  catch (const std::exception& e)
  {
    QMessageBox::information(this, tr("TIN Generation"), e.what());
    return;
  }

  m_ui->m_newLayerNameLineEdit->setText(fileDialog.getFileName().c_str());
  m_ui->m_repositoryLineEdit->setText(fileDialog.getPath().c_str());

  m_toFile = true;
  m_ui->m_newLayerNameLineEdit->setEnabled(false);
}

void te::mnt::TINGenerationDialog::onOkPushButtonClicked()
{
  //progress
  te::qt::widgets::ProgressViewerDialog v(this);

  try
  {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (!m_isolinesLayer.get() && !m_samplesLayer.get())
      throw te::common::Exception(TE_TR("Select a input layer."));

    if (m_ui->m_yesradioButton->isChecked() && !m_breaklinesLayer.get())
      throw te::common::Exception(TE_TR("Select a breakline layer."));

    bool ok;
    m_edgeSize = m_ui->m_minedgelineEdit->text().toDouble(&ok);
    if (!ok)
      throw te::common::Exception(TE_TR("Define a Minimal Edge Size."));

    te::mnt::TINGeneration *Tin = new te::mnt::TINGeneration();

    // Checking consistency of the input layer where the buffer will executed
    if (m_isolinesLayer.get())
    {
      m_tol = m_ui->m_tollineEdit->text().toDouble(&ok);
      if (!ok)
        throw te::common::Exception(TE_TR("Define a isolines tolerance."));

      m_distance = m_ui->m_distancelineEdit->text().toDouble(&ok);
      if (!ok)
        throw te::common::Exception(TE_TR("Define a distance of isolines points."));

      Tin->setInput(m_isolinesDataSource, m_isoSetName, m_isolinesDataSource->getDataSetType(m_isoSetName), te::mnt::Isolines);
    }
    if (m_samplesLayer.get())
    {
      Tin->setInput(m_samplesDataSource, m_sampleSetName, m_samplesDataSource->getDataSetType(m_sampleSetName), te::mnt::Samples);
    }

    if (m_samplesLayer.get() && m_isolinesLayer.get())
    {
      if (m_isosrid != m_samplesrid)
        throw te::common::Exception(TE_TR("Different SRID."));
    }

    if (m_breaklinesLayer.get())
    {
      m_breaktol = m_ui->m_breaktollineEdit->text().toDouble(&ok);
      if (!ok)
        throw te::common::Exception(TE_TR("Define a breaklines tolerance."));

      te::map::DataSetLayer* dsbreaklineLayer = dynamic_cast<te::map::DataSetLayer*>(m_breaklinesLayer.get());
      if (!dsbreaklineLayer)
        throw te::common::Exception(TE_TR("Can not execute this operation on this type of layer."));

      te::da::DataSourcePtr inDataSource = te::da::GetDataSource(dsbreaklineLayer->getDataSourceId(), true);
      if (!inDataSource.get())
        throw te::common::Exception(TE_TR("The selected input data source can not be accessed."));

      std::string inDsetNamebreakline = dsbreaklineLayer->getDataSetName();
      Tin->setBreakLine(inDataSource, inDsetNamebreakline, inDataSource->getDataSetType(inDsetNamebreakline), m_breaktol);
    }

    // Checking consistency of output paramenters
    if (m_ui->m_repositoryLineEdit->text().isEmpty())
      throw te::common::Exception(TE_TR("Select a repository for the resulting layer."));

    if (m_ui->m_newLayerNameLineEdit->text().isEmpty())
      throw te::common::Exception(TE_TR("Define a name for the resulting layer."));

    std::string outputdataset = m_ui->m_newLayerNameLineEdit->text().toUtf8().data();

    std::string dsinfo("file://");
    boost::filesystem::path uri(m_ui->m_repositoryLineEdit->text().toUtf8().data());

    if (m_toFile)
    {
      if (te::core::FileSystem::exists(uri.string()))
        throw te::common::Exception(TE_TR("Output file already exists. Remove it or select a new name and try again."));

      std::size_t idx = outputdataset.find(".");
      if (idx != std::string::npos)
        outputdataset = outputdataset.substr(0, idx);

      dsinfo += uri.string();

      te::da::DataSourcePtr dsOGR(te::da::DataSourceFactory::make("OGR", dsinfo).release());
      dsOGR->open();

      if (dsOGR->dataSetExists(outputdataset))
        throw te::common::Exception(TE_TR("There is already a dataset with the requested name in the output data source. Remove it or select a new name and try again."));

      Tin->setOutput(dsOGR, outputdataset);
    }
    else
    {
      te::da::DataSourcePtr aux = te::da::GetDataSource(m_outputDatasource->getId());
      if (!aux)
        throw te::common::Exception(TE_TR("The selected output datasource can not be accessed."));

      if (aux->dataSetExists(outputdataset))
        throw te::common::Exception(TE_TR("There is already a dataset with the requested name in the output data source. Remove it or select a new name and try again."));

      Tin->setOutput(aux, outputdataset);
    }

    Tin->setSRID(m_outsrid);

    if (m_outsrid>0)
    {
      te::common::UnitOfMeasurePtr unitin = te::srs::SpatialReferenceSystemManager::getInstance().getUnit((unsigned int)m_outsrid);
      if (unitin.get())
      {
        te::common::UnitOfMeasurePtr unitout = te::common::UnitsOfMeasureManager::getInstance().find("metre");

        if (unitin->getId() != te::common::UOM_Metre)
        {
          convertPlanarToAngle(m_tol, unitout);
          convertPlanarToAngle(m_distance, unitout);
          convertPlanarToAngle(m_edgeSize, unitout);
        }
      }
    }

    Tin->setParams(m_tol, m_distance, m_edgeSize, m_ui->m_isolinesZcomboBox->currentText().toUtf8().data(), m_ui->m_samplesZcomboBox->currentText().toUtf8().data());

    int method = m_ui->m_typecomboBox->currentIndex();
    Tin->setMethod(method);

    bool result = Tin->run();

    if (result)
    {
      if (m_toFile)
      {
        // let's include the new datasource in the managers
        boost::uuids::basic_random_generator<boost::mt19937> gen;
        boost::uuids::uuid u = gen();
        std::string id = boost::uuids::to_string(u);

        te::da::DataSourceInfoPtr ds(new te::da::DataSourceInfo);
        ds->setConnInfo(dsinfo);
        ds->setTitle(uri.stem().string());
        ds->setAccessDriver("OGR");
        ds->setType("OGR");
        ds->setDescription(uri.string());
        ds->setId(id);

        te::da::DataSourcePtr newds = te::da::DataSourceManager::getInstance().get(id, "OGR", ds->getConnInfo());
        newds->open();
        te::da::DataSourceInfoManager::getInstance().add(ds);
        m_outputDatasource = ds;
      }

      // creating a layer for the result
      te::da::DataSourcePtr outDataSource = te::da::GetDataSource(m_outputDatasource->getId());

      te::qt::widgets::DataSet2Layer converter(m_outputDatasource->getId());

      te::da::DataSetTypePtr dt(outDataSource->getDataSetType(outputdataset).release());
      m_outputLayer = converter(dt);
    }

    delete Tin;
   }
   catch (const std::exception& e)
    {
      QApplication::restoreOverrideCursor();
      QMessageBox::information(this, tr("TIN Generation"), e.what());
      return;
    }
   QApplication::restoreOverrideCursor();
   accept();
}

void te::mnt::TINGenerationDialog::onCancelPushButtonClicked()
{
  reject();
}

te::map::AbstractLayerPtr te::mnt::TINGenerationDialog::getLayer()
{
  return m_outputLayer;
}

void te::mnt::TINGenerationDialog::onSrsToolButtonClicked()
{
  te::qt::widgets::SRSManagerDialog srsDialog(this);
  srsDialog.setWindowTitle(tr("Choose the SRS"));

  if (srsDialog.exec() == QDialog::Rejected)
    return;

  int newSRID = srsDialog.getSelectedSRS().first;

  setSRID(newSRID);
 
}

void te::mnt::TINGenerationDialog::setSRID(int newSRID)
{
  if (newSRID <= 0)
  {
    m_ui->m_resSRIDLabel->setText("No SRS defined");
  }
  else
  {
    std::string name = te::srs::SpatialReferenceSystemManager::getInstance().getName(newSRID);
    if (name.size())
      m_ui->m_resSRIDLabel->setText(name.c_str());
    else
      m_ui->m_resSRIDLabel->setText(QString("%1").arg(newSRID));
  }
  m_outsrid = newSRID;

}
