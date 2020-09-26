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
\file terralib/mnt/qt/MNTGenerationDialog.cpp

\brief A dialog for Retangular Grid generation
*/

//terralib
#include "../../core/filesystem/FileSystem.h"
#include "../../core/logger/Logger.h"
#include "../../core/translator/Translator.h"
#include "../../common/Exception.h"
#include "../../common/progress/ProgressManager.h"
#include "../../dataaccess/datasource/DataSourceFactory.h"
#include "../../dataaccess/datasource/DataSourceInfoManager.h"
#include "../../dataaccess/datasource/DataSourceManager.h"
#include "../../dataaccess/utils/Utils.h"
#include "../../geometry/GeometryProperty.h"
#include "../../maptools/DataSetLayer.h"
#include "../../maptools/RasterContrast.h"
#include "../../maptools/Utils.h"
#include "../../qt/widgets/datasource/selector/DataSourceSelectorDialog.h"
#include "../../qt/widgets/progress/ProgressViewerDialog.h"
#include "../../qt/widgets/rp/Utils.h"
#include "../../qt/widgets/srs/SRSManagerDialog.h"
#include "../../qt/widgets/Utils.h"
#include "../../qt/widgets/utils/FileDialog.h"
#include "../../raster.h"
#include "../../raster/Interpolator.h"
#include "../../raster/RasterFactory.h"
#include "../../rp/Contrast.h"
#include "../../srs/SpatialReferenceSystemManager.h"

#include "../core/CalculateGrid.h"
#include "../core/SplineGrass.h"
#include "../core/SplineGrassMitasova.h"
#include "../core/TINCalculateGrid.h"
#include "../core/Utils.h"

#include "LayerSearchDialog.h"
#include "MNTGenerationDialog.h"
#include "ui/ui_MNTGenerationDialogForm.h"

// Qt
#include <QFileDialog>
#include <QMessageBox>

// BOOST
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

te::mnt::MNTGenerationDialog::MNTGenerationDialog(QWidget* parent , Qt::WindowFlags f)
  : QDialog(parent, f),
  m_ui(new Ui::MNTGenerationDialogForm),
  m_layers(std::list<te::map::AbstractLayerPtr>())
{
  // add controls
  m_ui->setupUi(this);

  //signals
  connect(m_ui->m_vectorradioButton, SIGNAL(toggled(bool)), this, SLOT(onVectorToggled()));
  connect(m_ui->m_gridradioButton, SIGNAL(toggled(bool)), this, SLOT(onGridToggled()));
  connect(m_ui->m_isolinesSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputIsolinesToolButtonClicked()));
  connect(m_ui->m_isolinescomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onIsolinesComboBoxChanged(int)));
  connect(m_ui->m_sampleSearchToolButton, SIGNAL(clicked()), this, SLOT(onInputSamplesToolButtonClicked()));
  connect(m_ui->m_samplescomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onSamplesComboBoxChanged(int)));
  connect(m_ui->m_layersComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onInputComboBoxChanged(int)));
  connect(m_ui->m_layerSearchToolButton, SIGNAL(clicked()), this, SLOT(onlayerSearchToolButtonClicked()));
  
  connect(m_ui->m_dummycheckBox, SIGNAL(toggled(bool)), m_ui->m_dummylineEdit, SLOT(setEnabled(bool)));

  connect(m_ui->m_interpolatorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(oninterpolatorComboBoxChanged(int)));

  m_ui->m_dimLLineEdit->setValidator(new QIntValidator(this));
  m_ui->m_dimCLineEdit->setValidator(new QIntValidator(this));
  m_ui->m_resXLineEdit->setValidator(new QDoubleValidator(this));
  m_ui->m_resYLineEdit->setValidator(new QDoubleValidator(this));

  connect(m_ui->m_resXLineEdit, SIGNAL(editingFinished()), this, SLOT(onResXLineEditEditingFinished()));
  connect(m_ui->m_resYLineEdit, SIGNAL(editingFinished()), this, SLOT(onResYLineEditEditingFinished()));
  connect(m_ui->m_dimCLineEdit, SIGNAL(editingFinished()), this, SLOT(onDimCLineEditEditingFinished()));
  connect(m_ui->m_dimLLineEdit, SIGNAL(editingFinished()), this, SLOT(onDimLLineEditEditingFinished()));

  m_ui->m_targetDatasourceToolButton->setIcon(QIcon::fromTheme("datasource"));
  connect(m_ui->m_targetFileToolButton, SIGNAL(clicked()), this, SLOT(onTargetFileToolButtonPressed()));
  connect(m_ui->m_targetDatasourceToolButton, SIGNAL(clicked()), this, SLOT(onTargetDatasourceToolButtonPressed()));

  connect(m_ui->m_okPushButton, SIGNAL(clicked()), this, SLOT(onOkPushButtonClicked()));
  connect(m_ui->m_cancelPushButton, SIGNAL(clicked()), this, SLOT(onCancelPushButtonClicked()));

  m_ui->m_helpPushButton->setNameSpace("dpi.inpe.br.plugins");
  m_ui->m_helpPushButton->setPageReference("plugins/mnt/DTM_DTM.html");

  m_ui->m_srsToolButton->setIcon(QIcon::fromTheme("srs"));
  connect(m_ui->m_srsToolButton, SIGNAL(clicked()), this, SLOT(onSrsToolButtonClicked()));

  for (int i = 2; i < 10; i++)
    m_ui->m_powerComboBox->addItem(QString::number(i));

  //Default Values
  m_ui->m_tensionLineEdit->setText("40");
  m_ui->m_smothLineEdit->setText("0.1");
  m_ui->m_minPtsMitLineEdit->setText("50");

  m_outsrid = 0;

}

te::mnt::MNTGenerationDialog::~MNTGenerationDialog() = default;

void te::mnt::MNTGenerationDialog::setLayers(std::list<te::map::AbstractLayerPtr> layers)
{
  m_layers = layers;

  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  size_t nvect = 0;
  size_t ngrid = 0;

  while (it != m_layers.end())
  {
    if (it->get())
    {
      if (it->get()->isValid())
      {
        te::map::AbstractLayerPtr layer = it->get();
        std::unique_ptr<te::da::DataSetType> dsType(layer->getSchema());
        mntType inputType = getMNTType(dsType.get());
        if (inputType == GRID)
        {
          te::rst::RasterProperty* rasterProp = te::da::GetFirstRasterProperty(dsType.get());
          if (rasterProp->getBandProperties().size() > 1)
          {
            ++it;
            continue;
          }
        }
        if (inputType == GRID || inputType == TIN)
        {
          m_ui->m_layersComboBox->addItem(QString(layer->getTitle().c_str()), QVariant(layer->getId().c_str()));
          ngrid++;
        }
        if (inputType == SAMPLE)
        {
          m_ui->m_samplescomboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));
          nvect++;
        }
        if (inputType == ISOLINE)
        {
          m_ui->m_isolinescomboBox->addItem(QString(it->get()->getTitle().c_str()), QVariant(it->get()->getId().c_str()));
          nvect++;
        }
      }
    }
    ++it;
  }
  m_ui->m_layersComboBox->insertItem(0, "");
  m_ui->m_samplescomboBox->insertItem(0, "");
  m_ui->m_isolinescomboBox->insertItem(0, "");

  m_ui->m_layersComboBox->setCurrentIndex(0);
  m_ui->m_samplescomboBox->setCurrentIndex(0);
  m_ui->m_isolinescomboBox->setCurrentIndex(0);

  if (nvect > 0)
  {
    m_ui->m_vectorradioButton->setChecked(true);
    onVectorToggled();
  }
  else if (ngrid > 0)
  {
    m_ui->m_gridradioButton->setChecked(true);
    onGridToggled();
  }

}

te::map::AbstractLayerPtr te::mnt::MNTGenerationDialog::getLayer()
{
  return m_outputLayer;
}

void te::mnt::MNTGenerationDialog::onVectorToggled()
{
  if (m_ui->m_gridradioButton->isChecked())
    return;

  m_ui->m_inputtabWidget->setTabEnabled(0, true);
  m_ui->m_inputtabWidget->setTabEnabled(1, true);
  m_ui->m_inputtabWidget->setTabEnabled(2, false);

  m_ui->m_interpolatorComboBox->clear();

  m_inputType = SAMPLE;

  m_ui->m_interpolatorComboBox->addItem("Weighted Avg./Z Value/Quadrant");
  m_ui->m_interpolatorComboBox->addItem("Weighted Average/Quadrant");
  m_ui->m_interpolatorComboBox->addItem("Weighted Average");
  m_ui->m_interpolatorComboBox->addItem("Simple Average");
  m_ui->m_interpolatorComboBox->addItem("Nearest Neighbor");
  m_ui->m_interpolatorComboBox->addItem("Bilinear Spline");
  m_ui->m_interpolatorComboBox->addItem("Bicubic Spline");
  m_ui->m_interpolatorComboBox->addItem("Mitasova Spline");

 // m_ui->m_interparamStackedWidget->setEnabled(true);

  int index = m_sampleinputLayer ? m_ui->m_samplescomboBox->findText(m_sampleinputLayer->getTitle().c_str()) : 0;
  onSamplesComboBoxChanged(index);

  index = m_isoinputLayer ? m_ui->m_isolinescomboBox->findText(m_isoinputLayer->getTitle().c_str()) : 0;
  onIsolinesComboBoxChanged(index);

}

void te::mnt::MNTGenerationDialog::onGridToggled()
{
  if (!m_ui->m_gridradioButton->isChecked())
    return;

  m_ui->m_inputtabWidget->setTabEnabled(0, false);
  m_ui->m_inputtabWidget->setTabEnabled(1, false);
  m_ui->m_inputtabWidget->setTabEnabled(2, true);

  m_ui->m_interparamStackedWidget->setCurrentIndex(3);

  int index = m_inputLayer ? m_ui->m_layersComboBox->findText(m_inputLayer->getTitle().c_str()) : 0;

  onInputComboBoxChanged(index);
}

void  te::mnt::MNTGenerationDialog::onInputIsolinesToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(ISOLINE);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
    return;

  int index = m_ui->m_isolinescomboBox->findText(search.getLayer()->getTitle().c_str());
  m_ui->m_isolinescomboBox->setCurrentIndex(index);

}

void  te::mnt::MNTGenerationDialog::onIsolinesComboBoxChanged(int index)
{
  m_isoinputLayer = nullptr;

  m_ui->m_isolinesZcomboBox->clear();

  if (index <= 0)
    return;

  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_isolinescomboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();

  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_isoinputLayer = it->get();

      if (m_sampleinputLayer)
        if (m_isoinputLayer->getSRID() != m_sampleinputLayer->getSRID())
        {
          QMessageBox::warning(this, tr("DTM Generation"), tr("Input Layers with different SRIDs!"));
        }

      double width = m_sampleinputLayer ? m_sampleinputLayer->getExtent().getWidth() : m_isoinputLayer ? m_isoinputLayer->getExtent().getWidth() : 0;
      double height = m_sampleinputLayer ? m_sampleinputLayer->getExtent().getHeight() : m_isoinputLayer ? m_isoinputLayer->getExtent().getHeight() : 0;

      double raio = std::sqrt(width * width + height * height) / 5.;
      m_ui->m_radiusLineEdit->setText(QString::number(raio, 'f', 4));

      setSRID(m_isoinputLayer->getSRID());

      std::unique_ptr<te::da::DataSetType> dsType(m_isoinputLayer->getSchema());

      m_isoinDataSource = te::da::GetDataSource(m_isoinputLayer->getDataSourceId(), true);
      if (!m_isoinDataSource.get())
        return;

      onResXLineEditEditingFinished();
      onResYLineEditEditingFinished();

      m_isoinSetName = m_isoinputLayer->getDataSetName();

      std::unique_ptr<te::da::DataSet> inDset(m_isoinDataSource->getDataSet(m_isoinSetName));
      std::size_t geo_pos = te::da::GetFirstPropertyPos(inDset.get(), te::dt::GEOMETRY_TYPE);
      inDset->moveFirst();
      if (inDset->getGeometry(geo_pos)->is3D())
      {
        m_ui->m_isolinesZcomboBox->hide();
        m_ui->m_isolinesZlabel->hide();
        return;
      }

      m_ui->m_isolinesZcomboBox->show();
      m_ui->m_isolinesZlabel->show();
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
        default:
          break;
        }
      }

      break;
    }
    it++;
  }
}

void  te::mnt::MNTGenerationDialog::onInputSamplesToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(SAMPLE);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
    return;

  int index = m_ui->m_samplescomboBox->findText(search.getLayer()->getTitle().c_str());
  m_ui->m_samplescomboBox->setCurrentIndex(index);
}

void  te::mnt::MNTGenerationDialog::onSamplesComboBoxChanged(int index)
{
  m_sampleinputLayer = nullptr;

  m_ui->m_samplesZcomboBox->clear();
  m_ui->m_samplesZcomboBox->hide();
  m_ui->m_samplesZlabel->hide();

  if (index <= 0)
    return;

  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_samplescomboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();

  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_sampleinputLayer = it->get();

      if (m_isoinputLayer)
        if (m_isoinputLayer->getSRID() != m_sampleinputLayer->getSRID())
        {
          QMessageBox::warning(this, tr("DTM Generation"), tr("Input Layers with different SRIDs!"));
        }

      double width = m_sampleinputLayer ? m_sampleinputLayer->getExtent().getWidth() : m_isoinputLayer ? m_isoinputLayer->getExtent().getWidth() : 0;
      double height = m_sampleinputLayer ? m_sampleinputLayer->getExtent().getHeight() : m_isoinputLayer ? m_isoinputLayer->getExtent().getHeight() : 0;

      double raio = std::sqrt(width * width + height * height) / 5.;
      m_ui->m_radiusLineEdit->setText(QString::number(raio, 'f', 4));

      setSRID(m_sampleinputLayer->getSRID());

      std::unique_ptr<te::da::DataSetType> dsType(m_sampleinputLayer->getSchema());

      m_sampleinDataSource = te::da::GetDataSource(m_sampleinputLayer->getDataSourceId(), true);
      if (!m_sampleinDataSource.get())
        return;

      m_sampleinSetName = m_sampleinputLayer->getDataSetName();

      onResXLineEditEditingFinished();
      onResYLineEditEditingFinished();

      std::unique_ptr<te::da::DataSet> inDset(m_sampleinDataSource->getDataSet(m_sampleinSetName));
      std::size_t geo_pos = te::da::GetFirstPropertyPos(inDset.get(), te::dt::GEOMETRY_TYPE);
      inDset->moveFirst();
      std::unique_ptr<te::gm::Geometry> gin(inDset->getGeometry(geo_pos));
      if (gin->is3D())
      {
        m_ui->m_samplesZcomboBox->hide();
        m_ui->m_samplesZlabel->hide();
        return;
      }

      m_ui->m_samplesZcomboBox->show();
      m_ui->m_samplesZlabel->show();
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
        default:
          break;
        }
      }
      break;
    }
    it++;
  }
}

void te::mnt::MNTGenerationDialog::onInputComboBoxChanged(int index)
{
  m_inputLayer = nullptr;

  if (index <= 0)
    return;

  m_ui->m_interpolatorComboBox->clear();
  std::list<te::map::AbstractLayerPtr>::iterator it = m_layers.begin();
  std::string layerID = m_ui->m_layersComboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();

  while (it != m_layers.end())
  {
    if(layerID == it->get()->getId())
    {
      m_inputLayer = it->get();

      setSRID(m_inputLayer->getSRID());

      m_inDataSource = te::da::GetDataSource(m_inputLayer->getDataSourceId(), true);
      if (!m_inDataSource.get())
        return;

      m_inSetName = m_inputLayer->getDataSetName();

      std::unique_ptr<te::da::DataSetType> dsType(m_inputLayer->getSchema());

      m_inputType = getMNTType(dsType.get());

      if (m_inputType == TIN)
      {
        te::da::DataSourcePtr ds = te::da::GetDataSource(m_inputLayer->getDataSourceId());
        m_ui->m_interpolatorComboBox->addItem("Linear");
        m_ui->m_interpolatorComboBox->addItem("Quintic without breaklines");

        std::string tname("type1");
        if (ds->propertyExists(m_inputLayer->getDataSetName(), tname))
        {
          std::string qry("Select type1, type2, type3 from ");
          qry += m_inputLayer->getTitle();
          qry += " where (type1 > 3 and type1 < 7) or (type2 > 3 and type2 < 7) or (type3 > 3 and type3 < 7)";
          std::unique_ptr<te::da::DataSet> dataquery(ds->query(qry));
          if (!dataquery->isEmpty())
            m_ui->m_interpolatorComboBox->addItem("Quintic with breaklines");
        }
        m_ui->m_dummycheckBox->setVisible(false);
        m_ui->m_dummylineEdit->setVisible(false);
      }

      if (m_inputType == GRID)
      {
        m_inputType = GRID;
        m_ui->m_interpolatorComboBox->addItem("Bilinear");
        m_ui->m_interpolatorComboBox->addItem("Bicubic");

        std::size_t rpos = te::da::GetFirstPropertyPos(m_inputLayer->getData().get(), te::dt::RASTER_TYPE);
        std::unique_ptr<te::rst::Raster> inputRst(m_inputLayer->getData()->getRaster(rpos).release());
        m_ui->m_dummycheckBox->setVisible(true);
        m_ui->m_dummylineEdit->setVisible(true);
        m_ui->m_dummylineEdit->setText(QString::number(inputRst->getBand(0)->getProperty()->m_noDataValue));

      }
      onResXLineEditEditingFinished();
      onResYLineEditEditingFinished();
      break;
    }
    it++;
  }
}

void te::mnt::MNTGenerationDialog::onlayerSearchToolButtonClicked()
{
  LayerSearchDialog search(this->parentWidget());
  search.setLayers(m_layers);
  QList<mntType> types;
  types.append(GRID);
  types.append(TIN);
  search.setActive(types);

  if (search.exec() != QDialog::Accepted)
  {
    return;
  }

  int index = m_ui->m_layersComboBox->findText(search.getLayer()->getTitle().c_str());
  m_ui->m_layersComboBox->setCurrentIndex(index);
}

void te::mnt::MNTGenerationDialog::oninterpolatorComboBoxChanged(int index)
{
  std::string inter = m_ui->m_interpolatorComboBox->itemData(index, Qt::UserRole).toString().toUtf8().data();

  switch (m_inputType)
  {
    case SAMPLE:
    {
      switch (index)
      {
      case 0: //Weighted Average/Z Value/Quadrant
      case 1: //Weighted Average/Quadrant
      case 2: //Weighted Average
        m_ui->m_interparamStackedWidget->setCurrentIndex(0);
        m_ui->m_powerLabel->show();
        m_ui->m_powerComboBox->show();
        m_ui->m_radiusLabel->show();
        m_ui->m_radiusLineEdit->show();
        break;
      case 3: //Simple Average
      case 4: //Nearest Neighbor
        m_ui->m_interparamStackedWidget->setCurrentIndex(0);
        m_ui->m_powerLabel->hide();
        m_ui->m_powerComboBox->hide();
        m_ui->m_radiusLabel->show();
        m_ui->m_radiusLineEdit->show();
        break;
      case 5: //Bilinear Spline
      case 6: //Bicubic Spline
        m_ui->m_interparamStackedWidget->setCurrentIndex(1);
        break;
      case 7: //Mitasova Spline
        m_ui->m_interparamStackedWidget->setCurrentIndex(2);
        break;
      default:
        break;
      }
      break;
    }
    case TIN:
      m_ui->m_interparamStackedWidget->setCurrentIndex(3);
      switch (index)
      {
      case 0:
      case 1:
      case 2:
        break;
      default:
        break;
      }
      break;
    case GRID:
      m_ui->m_interparamStackedWidget->setCurrentIndex(3);
      switch (index)
      {
      case 0:
      case 1:
        break;
      default:
        break;
      }
      break;
    case OTHER:
    case ISOLINE:
    default:
      m_ui->m_interparamStackedWidget->setCurrentIndex(3);
      break;
  }
}

void te::mnt::MNTGenerationDialog::onResXLineEditEditingFinished()
{
  double resX = m_ui->m_resXLineEdit->text().toDouble();
  if (resX == 0.)
  {
    m_ui->m_dimCLineEdit->setText("");
    return;
  }

  te::map::AbstractLayerPtr inlayer = nullptr;
  if (m_ui->m_vectorradioButton->isChecked())
    inlayer = m_sampleinputLayer ? m_sampleinputLayer : m_isoinputLayer;
  else
    inlayer = m_inputLayer;

  te::gm::Envelope env = inlayer ? inlayer->getExtent() : te::gm::Envelope(0, 0, 0, 0);

  if (!env.isValid())
  {
    QMessageBox::warning(this, tr("DTM Generation"), tr("Invalid envelope!"));
    return;
  }

  int maxCols = (int)ceil((env.m_urx - env.m_llx) / resX);

  m_ui->m_dimCLineEdit->setText(QString::number(maxCols));
}

void te::mnt::MNTGenerationDialog::onResYLineEditEditingFinished()
{
  double resY = m_ui->m_resYLineEdit->text().toDouble();
  if (resY == 0.)
  {
    m_ui->m_dimLLineEdit->setText("");
    return;
  }

  te::map::AbstractLayerPtr inlayer = nullptr;
  if (m_ui->m_vectorradioButton->isChecked())
    inlayer = m_sampleinputLayer ? m_sampleinputLayer : m_isoinputLayer;
  else
    inlayer = m_inputLayer;

  te::gm::Envelope env = inlayer ? inlayer->getExtent() : te::gm::Envelope(0, 0, 0, 0);

  if (!env.isValid())
  {
    QMessageBox::warning(this, tr("DTM Generation"), tr("Invalid envelope!"));
    return;
  }

  int maxRows = (int)ceil((env.m_ury - env.m_lly) / resY);

  m_ui->m_dimLLineEdit->setText(QString::number(maxRows));
}

void te::mnt::MNTGenerationDialog::onDimLLineEditEditingFinished()
{
  int cols = m_ui->m_dimCLineEdit->text().toInt();
  if (cols == 0)
  {
    m_ui->m_resXLineEdit->setText("");
    return;
  }

  te::map::AbstractLayerPtr inlayer = nullptr;
  if (m_ui->m_vectorradioButton->isChecked())
    inlayer = m_sampleinputLayer ? m_sampleinputLayer : m_isoinputLayer;
  else
    inlayer = m_inputLayer;

  te::gm::Envelope env = inlayer ? inlayer->getExtent() : te::gm::Envelope(0, 0, 0, 0);

  if (!env.isValid())
  {
    QMessageBox::warning(this, tr("DTM Generation"), tr("Invalid envelope!"));
    return;
  }

  double resX = (env.m_urx - env.m_llx) / cols;

  m_ui->m_resXLineEdit->setText(QString::number(resX));
}

void te::mnt::MNTGenerationDialog::onDimCLineEditEditingFinished()
{
  int rows = m_ui->m_dimLLineEdit->text().toInt();
  if (rows == 0)
  {
    m_ui->m_resYLineEdit->setText("");
    return;
  }

  te::map::AbstractLayerPtr inlayer = nullptr;
  if (m_ui->m_vectorradioButton->isChecked())
    inlayer = m_sampleinputLayer ? m_sampleinputLayer : m_isoinputLayer;
  else
    inlayer = m_inputLayer;

  te::gm::Envelope env = inlayer ? inlayer->getExtent() : te::gm::Envelope(0, 0, 0, 0);

  if (!env.isValid())
  {
    QMessageBox::warning(this, tr("DTM Generation"), tr("Invalid envelope!"));
    return;
  }

  double resY = (env.m_ury - env.m_lly) / rows;

  m_ui->m_resYLineEdit->setText(QString::number(resY));
}

void te::mnt::MNTGenerationDialog::onTargetFileToolButtonPressed()
{
  m_ui->m_newLayerNameLineEdit->clear();
  m_ui->m_repositoryLineEdit->clear();

  te::qt::widgets::FileDialog fileDialog(this, te::qt::widgets::FileDialog::RASTER);

  try {
    fileDialog.exec();
  }
  catch (const std::exception& e)
  {
    QMessageBox::information(this, tr("DTM Generation"), e.what());
    return;
  }

  m_ui->m_repositoryLineEdit->setText(fileDialog.getPath().c_str());
  m_ui->m_newLayerNameLineEdit->setText(fileDialog.getFileName().c_str());

  m_ui->m_newLayerNameLineEdit->setEnabled(false);
}

void te::mnt::MNTGenerationDialog::onTargetDatasourceToolButtonPressed()
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

}

void te::mnt::MNTGenerationDialog::onOkPushButtonClicked()
{
  //progress
  te::qt::widgets::ProgressViewerDialog v(this);

  try
  {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    if (!m_inputLayer.get() && !m_isoinputLayer.get() && !m_sampleinputLayer.get())
      throw te::common::Exception(TE_TR("Select a input layer."));

    std::map<std::string, std::string> outdsinfo;
    std::string inDsetName = m_inSetName;

    // Checking consistency of output paramenters
    if (m_ui->m_repositoryLineEdit->text().isEmpty())
      throw te::common::Exception(TE_TR("Select a repository for the resulting layer."));

    if (m_ui->m_newLayerNameLineEdit->text().isEmpty())
      throw te::common::Exception(TE_TR("Define a name for the resulting layer."));

    std::string outputdataset = m_ui->m_newLayerNameLineEdit->text().toUtf8().data();
    boost::filesystem::path uri(m_ui->m_repositoryLineEdit->text().toUtf8().data());

    if (te::core::FileSystem::exists(uri.string()))
      throw te::common::Exception(TE_TR("Output file already exists. Remove it or select a new name and try again."));

    std::size_t idx = outputdataset.find(".");
    if (idx != std::string::npos)
      outputdataset = outputdataset.substr(0, idx);

    outdsinfo["URI"] = uri.string();

    double radius = m_ui->m_radiusLineEdit->text().toDouble();
    int pow = m_ui->m_powerComboBox->currentText().toInt();

    bool ok;
    double resxo = m_ui->m_resXLineEdit->text().toDouble(&ok);
    if (!ok)
      throw te::common::Exception(TE_TR("Define X resolution."));
    double resyo = m_ui->m_resYLineEdit->text().toDouble(&ok);
    if (!ok)
      throw te::common::Exception(TE_TR("Define Y resolution."));

    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();

    switch (m_inputType)
    {
      case SAMPLE:
      {
        int inter_i = m_ui->m_interpolatorComboBox->currentIndex();
        bool spline = false;
        switch (inter_i)
        {
        case 0://Weighted Avg./Z Value/Quadrant
          m_inter = MediaCotaQuad;
          break;
        case 1://Weighted Average/Quadrant
          m_inter = MediaQuad;
          break;
        case 2://Weighted Average;
          m_inter = MediaPonderada;
          break;
        case 3://Simple Average;
          m_inter = Media;
          break;
        case 4://Nearest Neighbor
          m_inter = Vizinho;
          break;
        case 5://Bilinear Spline
          m_inter = SplineBilinear;
          spline = true;
          break;
        case 6://Bicubic Spline
          m_inter = SplineBicubico;
          spline = true;
          break;
        case 7://Mitasova Spline
          m_inter = Mitasova;
          spline = true;
          break;
        default:
          break;
        }

        if (!spline)
        {
          te::mnt::CalculateGrid *grid = new te::mnt::CalculateGrid();

          if (m_isoinputLayer.get())
          {
            grid->setInput(m_isoinDataSource, m_isoinSetName, m_isoinDataSource->getDataSetType(m_isoinSetName), te::mnt::Isolines);
          }
          if (m_sampleinputLayer.get())
          {
            grid->setInput(m_sampleinDataSource, m_sampleinSetName, m_sampleinDataSource->getDataSetType(m_sampleinSetName), te::mnt::Samples);
          }
          grid->setOutput(outdsinfo);
          grid->setParams(m_ui->m_isolinesZcomboBox->currentText().toUtf8().data(), m_ui->m_samplesZcomboBox->currentText().toUtf8().data(), resxo, resyo, m_inter, radius, pow);
          grid->setSRID(m_outsrid);

          grid->run();

          min = grid->getMin();
          max = grid->getMax();

          delete grid;
        }
        else if (m_inter == Mitasova)
        {
          int mp = m_ui->m_minPtsMitLineEdit->text().toInt();
          double t = m_ui->m_tensionLineEdit->text().toDouble();
          double s = m_ui->m_smothLineEdit->text().toDouble();
          te::mnt::SplineInterpolationGrassMitasova *grid = new te::mnt::SplineInterpolationGrassMitasova(mp, t, s);

          if (m_isoinputLayer.get())
          {
            grid->setInput(m_isoinDataSource, m_isoinSetName, m_isoinDataSource->getDataSetType(m_isoinSetName), te::mnt::Isolines);
          }

          if (m_sampleinputLayer.get())
          {
            grid->setInput(m_sampleinDataSource, m_sampleinSetName, m_sampleinDataSource->getDataSetType(m_sampleinSetName), te::mnt::Samples);
          }

          grid->setOutput(outdsinfo);
          grid->setParams(m_ui->m_isolinesZcomboBox->currentText().toUtf8().data(), m_ui->m_samplesZcomboBox->currentText().toUtf8().data(), resxo, resyo, m_inter, radius, pow);
          grid->setSRID(m_outsrid);

          grid->calculateGrid();

          min = grid->getMin();
          max = grid->getMax();

          delete grid;

        }
        else
        {
          unsigned int px = m_ui->m_sepXSpinBox->text().toUInt();
          unsigned int py = m_ui->m_sepYSpinBox->text().toUInt();
          unsigned int mp = m_ui->m_minPtsSpinBox->text().toUInt();
          double ov = m_ui->m_overDoubleSpinBox->text().toDouble();

          te::mnt::SplineInterpolationGrass *grid = new te::mnt::SplineInterpolationGrass(px, py, mp, ov);

          if (m_isoinputLayer.get())
          {
            grid->setInput(m_isoinDataSource, m_isoinSetName, m_isoinDataSource->getDataSetType(m_isoinSetName), te::mnt::Isolines);
          }
          if (m_sampleinputLayer.get())
          {
            grid->setInput(m_sampleinDataSource, m_sampleinSetName, m_sampleinDataSource->getDataSetType(m_sampleinSetName), te::mnt::Samples);
          }
          grid->setOutput(outdsinfo);
          grid->setParams(m_ui->m_isolinesZcomboBox->currentText().toUtf8().data(), m_ui->m_samplesZcomboBox->currentText().toUtf8().data(), resxo, resyo, m_inter, radius, pow);
          grid->setSRID(m_outsrid);

          grid->generateGrid();

          min = grid->getMin();
          max = grid->getMax();

          delete grid;

        }
        break;
      }
      case TIN:
      {
        int inter_i = m_ui->m_interpolatorComboBox->currentIndex();
        switch (inter_i)
        {
        case 0: //Linear
          m_inter = Linear;
          break;
        case 1: //Quintico
          m_inter = Quintico;
            break;
        case 2: //Quintico breakline
          m_inter = QuinticoBrkLine;
        }

        te::mnt::TINCalculateGrid *grid = new te::mnt::TINCalculateGrid();

        grid->setInput(m_inDataSource, m_inSetName, m_inDataSource->getDataSetType(m_inSetName));
        grid->setOutput(outdsinfo);
        grid->setParams(resxo, resyo, m_inter);
        grid->setSRID(m_outsrid);

        grid->run();

        min = grid->getMin();
        max = grid->getMax();

        delete grid;
        break;
      }
      case GRID:
      {
        //get input raster
        std::unique_ptr<te::da::DataSet> inds(m_inputLayer->getData());
        std::size_t rpos = te::da::GetFirstPropertyPos(inds.get(), te::dt::RASTER_TYPE);
        std::unique_ptr<te::rst::Raster> inputRst(inds->getRaster(rpos).release());
        if (inputRst->getNumberOfBands() > 1)
        {
          throw te::common::Exception(TE_TR("Layer isn't Regular Grid."));
        }

        std::vector< std::complex<double> > dummy;
        if (m_ui->m_dummycheckBox->isChecked())
        {
          bool ok;
          dummy.push_back(m_ui->m_dummylineEdit->text().toDouble(&ok));
          if (!ok)
            throw te::common::Exception(TE_TR("Define Dummy Value."));
        }
        else
          dummy.push_back(inputRst->getBand(0)->getProperty()->m_noDataValue);

        int inter_i = m_ui->m_interpolatorComboBox->currentIndex();
        int inter = 0;
        switch (inter_i)
        {
        case 0: //Bilinear
          inter = te::rst::Bilinear;
          break;
        case 1: //Bicubico
          inter = te::rst::Bicubic;
          break;
        }

        te::rst::Interpolator interp(inputRst.get(), inter, dummy);
        double resxi = inputRst->getResolutionX();
        double resyi = inputRst->getResolutionY();
        unsigned int outputWidth = m_ui->m_dimCLineEdit->text().toUInt();
        unsigned int outputHeight = m_ui->m_dimLLineEdit->text().toUInt();
        int X1 = inputRst->getExtent()->getLowerLeftX();
        int Y2 = inputRst->getExtent()->getUpperRightY();
        te::gm::Coord2D ulc(X1, Y2);
        te::rst::Grid* grid = new te::rst::Grid(outputWidth, outputHeight, resxo, resyo, &ulc, m_outsrid);

        std::vector<te::rst::BandProperty*> bands;
        bands.push_back(new te::rst::BandProperty(0, te::dt::DOUBLE_TYPE, "DTM GRID"));
        bands[0]->m_nblocksx = 1;
        bands[0]->m_nblocksy = (int)outputHeight;
        bands[0]->m_blkw = (int)outputWidth;
        bands[0]->m_blkh = 1;
        bands[0]->m_colorInterp = te::rst::GrayIdxCInt;

        te::common::TaskProgress task("Calculating DTM...", te::common::TaskProgress::UNDEFINED, (int)(outputHeight*outputWidth));

        // create raster
        std::unique_ptr<te::rst::Raster> outRst(te::rst::RasterFactory::make("GDAL", grid, bands, outdsinfo));
        te::rst::Raster* out = outRst.get();

        std::vector<std::complex<double> > value;
        double xi, yi, xo, yo;

        for (unsigned int l = 0; l < outputHeight; l++)
        {
          for (unsigned int c = 0; c < outputWidth; c++)
          {
            if (!task.isActive())
              throw te::common::Exception(TE_TR("Canceled by user"));

            task.pulse();
            // Calculate the x and y coordinates of (l,c) corner of the output grid
            xo = (X1 + c * resxo + resxo / 2.);
            yo = (Y2 - l * resyo - resyo / 2.);

            // Calculate position of point (xs,ys) in the input grid
            xi = ((xo - (X1 + resxi / 2.)) / resxi);
            yi = (((Y2 - resyi / 2.) - yo) / resyi);
        
            interp.getValues(xi, yi, value);

            if (value != dummy)
            {
              max = (value[0].real() > max) ? value[0].real() : max;
              min = (value[0].real() < min) ? value[0].real() : min;
            }
            out->setValues(c, l, value);
          }
        }
        break;
      }
      case ISOLINE:
      case OTHER:
      default:
        break;
    }

    m_outputLayer = te::qt::widgets::createLayer("GDAL", outdsinfo);

    std::unique_ptr<te::rst::Raster> rst(te::map::GetRaster(m_outputLayer.get()));

    te::qt::widgets::applyRasterMultiResolution(tr("DTM Generation"), rst.get());

    te::map::RasterContrast* contrast = new te::map::RasterContrast(te::map::RasterTransform::LINEAR_CONTRAST, rst->getNumberOfBands());

    (m_outputLayer.get())->setRasterContrast(contrast);
    for (size_t b = 0; b < rst->getNumberOfBands(); ++b)
    {
      double gain, offset1, offset2;
      if (te::rp::Contrast::getGainAndOffset(te::rp::Contrast::InputParameters::LinearContrastT, min, max, 0., 255., gain, offset1, offset2))
      {
        contrast->setValues(gain, offset1, offset2, min, max, b);
      }
    }
  }
  catch (te::common::Exception& e)
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, "DTM Generation", e.what());
    return;
  }

  QApplication::restoreOverrideCursor();
  accept();
}

void te::mnt::MNTGenerationDialog::onCancelPushButtonClicked()
{
  reject();
}

void te::mnt::MNTGenerationDialog::onSrsToolButtonClicked()
{
  te::qt::widgets::SRSManagerDialog srsDialog(this);
  srsDialog.setWindowTitle(tr("Choose the SRS"));

  if (srsDialog.exec() == QDialog::Rejected)
    return;

  int newSRID = srsDialog.getSelectedSRS().first;

  setSRID(newSRID);

}

void te::mnt::MNTGenerationDialog::setSRID(int newSRID)
{
  if (newSRID <= 0)
  {
    m_ui->m_resSRIDLabel->setText("No SRS defined");
  }
  else
  {
    std::string name = te::srs::SpatialReferenceSystemManager::getInstance().getName((unsigned int)newSRID);
    if (name.size())
      m_ui->m_resSRIDLabel->setText(name.c_str());
    else
      m_ui->m_resSRIDLabel->setText(QString("%1").arg(newSRID));
  }
  m_outsrid = newSRID;

}
