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
  \file terralib/qt/widgets/externaltable/TableLinkDialog.h

  \brief A Qt dialog that allows users to create a new query layer based on the information of two distinct datasets
*/

// TerraLib
#include "../../../core/translator/Translator.h"
#include "../../../dataaccess/dataset/PrimaryKey.h"
#include "../../../dataaccess/datasource/DataSourceManager.h"
#include "../../../dataaccess/Enums.h"
#include "../../../dataaccess/query/BinaryFunction.h"
#include "../../../dataaccess/query/DataSetName.h"
#include "../../../dataaccess/query/Expression.h"
#include "../../../dataaccess/query/Field.h"
#include "../../../dataaccess/query/Join.h"
#include "../../../dataaccess/query/JoinConditionOn.h"
#include "../../../dataaccess/query/PropertyName.h"
#include "../../../dataaccess/utils/Utils.h"
#include "../../../geometry/GeometryProperty.h"
#include "../../../maptools/QueryLayer.h"
#include "../../../memory/DataSet.h"
#include "../../../qt/widgets/utils/ScopedCursor.h"
#include "../../../se/Utils.h"
#include "../table/DataSetTableView.h"
#include "FieldsDialog.h"
#include "TableLinkDialog.h"
#include "ui_TableLinkDialogForm.h"

// Boost
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

//QT
#include <QMessageBox>

// STL
#include <cassert>

te::qt::widgets::TableLinkDialog::TableLinkDialog(QWidget* parent, Qt::WindowFlags f)
  : QDialog(parent, f),
    m_ui(new Ui::TableLinkDialogForm)
{
  m_ui->setupUi(this);
  m_ui->m_dataToolButton->setIcon(QIcon::fromTheme("view-data-table"));
  m_ui->m_dataToolButton->setToolTip(tr("View dataset rows"));

  //Adjusting the dataSetTableView that will be used to display the tabular dataset's data
  m_tabularView.reset(new DataSetTableView(m_ui->m_tabularFrame));
  QGridLayout* tabularLayout = new QGridLayout(m_ui->m_tabularFrame);
  tabularLayout->addWidget(m_tabularView.get());
  tabularLayout->setContentsMargins(0, 0, 0, 0);

  m_tabularView->setAlternatingRowColors(true);
  m_tabularView->verticalHeader()->setVisible(false);
  m_tabularView->setSelectionMode(QAbstractItemView::NoSelection);
  m_tabularView->hide();
  m_ui->m_dataPreviewGroupBox->hide();
  m_ui->m_helpPushButton->setPageReference("widgets/external_table/table_link_dialog.html");

  //Connecting signals and slots
  connect(m_ui->m_dataSet2ComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDataCBIndexChanged(int)));
  connect(m_ui->m_dataToolButton, SIGNAL(clicked()), this, SLOT(onDataToolButtonnClicked()));
  connect(m_ui->m_OkPushButton, SIGNAL(clicked()), this, SLOT(onOkPushButtonClicked()));
  
}

te::qt::widgets::TableLinkDialog::~TableLinkDialog() = default;

void te::qt::widgets::TableLinkDialog::setInputLayer(te::map::AbstractLayerPtr inLayer)
{
  m_inputLayer.reset((te::map::DataSetLayer*)inLayer.get());
  m_ui->m_dataSet1LineEdit->setText(QString::fromUtf8(m_inputLayer->getDataSetName().c_str()));
  m_ds = te::da::DataSourceManager::getInstance().find(m_inputLayer->getDataSourceId());

  if(!m_ds->isOpened())
    m_ds->open();

  getDataSets();
}

te::da::Join* te::qt::widgets::TableLinkDialog::getJoin()
{
  size_t pos = m_inputLayer->getDataSetName().find(".");
  std::string inputAlias = m_inputLayer->getDataSetName().substr(pos + 1, m_inputLayer->getDataSetName().size() - 1);

  if(pos != std::string::npos)
    inputAlias = m_inputLayer->getDataSetName().substr(pos + 1, m_inputLayer->getDataSetName().size() - 1);
  else
    inputAlias = m_inputLayer->getDataSetName();

  te::da::DataSetName* inField  = new te::da::DataSetName(m_inputLayer->getDataSetName(), inputAlias);
  te::da::DataSetName* tabField = new te::da::DataSetName(m_ui->m_dataSet2ComboBox->currentText().toUtf8().data(), m_ui->m_dataSetAliasLineEdit->text().toUtf8().data());

  te::da::Expression* exp1 = new te::da::PropertyName(m_ui->m_dataset1ColumnComboBox->currentText().toUtf8().data());
  te::da::Expression* exp2 = new te::da::PropertyName(m_ui->m_dataset2ColumnComboBox->currentText().toUtf8().data());
  te::da::Expression* expression = new te::da::BinaryFunction("=", exp1, exp2);

  te::da::JoinType type = te::da::LEFT_JOIN;

  te::da::Join* join = new te::da::Join(inField, tabField, type, new te::da::JoinConditionOn(expression));
  return join;
}

te::da::Select te::qt::widgets::TableLinkDialog::getSelectQuery()
{
  //fields
  te::da::Fields* fields = new te::da::Fields;

  for (const std::string& unique : m_uniqueProps)
  {
    //Primary keys properties will have aliases to identify the original dataset
    te::da::Field* f = new te::da::Field(unique, "\"" + unique + "\"");

    fields->push_back(f);
  }

  for (const std::string& prop : m_properties)
  {
    te::da::Field* f = new te::da::Field(prop);
    fields->push_back(f);
  }

  //from
  te::da::From* from = new te::da::From;

  //Join
  from->push_back(getJoin());

  //build the select object
  te::da::Select s(fields, from);

  return s;
}

te::map::AbstractLayerPtr te::qt::widgets::TableLinkDialog::getQueryLayer()
{
  te::qt::widgets::ScopedCursor c(Qt::WaitCursor);

  static boost::uuids::basic_random_generator<boost::mt19937> gen;
  boost::uuids::uuid u = gen();
  std::string id = boost::uuids::to_string(u);

  std::string title = m_ui->m_layerTitleLineEdit->text().toUtf8().data();

  te::map::QueryLayerPtr layer(new te::map::QueryLayer(id, title));
  layer->setDataSourceId(m_ds->getId());
  layer->setDataSetName(m_inputLayer->getDataSetName());
  layer->setRendererType("QUERY_LAYER_RENDERER");
  layer->setQuery(new te::da::Select(getSelectQuery()));

  // SRID
  std::unique_ptr<const te::map::LayerSchema> schema(layer->getSchema());
  te::gm::GeometryProperty* gp = te::da::GetFirstGeomProperty(schema.get());
  if(gp)
  {
    layer->setSRID(gp->getSRID());
    layer->computeExtent();

    // style
    layer->setStyle(te::se::CreateFeatureTypeStyle(gp->getGeometryType()));
  }

  return layer;
}

void te::qt::widgets::TableLinkDialog::getDataSets()
{
  te::qt::widgets::ScopedCursor c(Qt::WaitCursor);

  std::string dsId = m_ds->getId();

  std::vector<std::string> datasetNames;

  te::da::GetDataSetNames(datasetNames, dsId);

  for (size_t i = 0; i < datasetNames.size(); i++)
  {
    std::unique_ptr<te::da::DataSetType> dataType(te::da::GetDataSetType(datasetNames[i], dsId));

    //The layer on the right side of the join must be different than the first and can not have a geometry
    if((datasetNames[i] != m_inputLayer->getDataSetName()) && (!dataType->hasGeom()))
      m_ui->m_dataSet2ComboBox->addItem(QString::fromUtf8(datasetNames[i].c_str()));
  }

  if (m_ui->m_dataSet2ComboBox->count() == 0)
  {
    throw te::common::Exception(TE_TR("No tabular data available to be linked in the selected Data Source."));
    return;
  }

  std::string DsName = m_ui->m_dataSet2ComboBox->currentText().toUtf8().data();
  size_t pos = DsName.find(".");
  if(pos != std::string::npos)
    m_ui->m_dataSetAliasLineEdit->setText(QString::fromUtf8(DsName.substr(pos + 1, DsName.size() - 1).c_str()));
  else
    m_ui->m_dataSetAliasLineEdit->setText(QString::fromUtf8(DsName.c_str()));
}

void te::qt::widgets::TableLinkDialog::getProperties()
{
  m_uniqueProps.clear();
  m_properties.clear();

  te::qt::widgets::ScopedCursor c(Qt::WaitCursor);

  //Clearing contents
  int index = m_ui->m_dataset1ColumnComboBox->currentIndex();
  m_ui->m_dataset1ColumnComboBox->clear();
  m_ui->m_dataset2ColumnComboBox->clear();

  //get the dataset names
  std::vector<std::string> datasetNames = m_ds->getDataSetNames();
  std::vector<std::pair<std::string, std::string> > dataSetSelecteds;
  std::string inputAlias;
  size_t pos = m_inputLayer->getDataSetName().find(".");

  if(pos != std::string::npos)
    inputAlias = m_inputLayer->getDataSetName().substr(pos + 1, m_inputLayer->getDataSetName().size() - 1);
  else
   inputAlias = m_inputLayer->getDataSetName();

  dataSetSelecteds.push_back(std::make_pair(m_inputLayer->getDataSetName(), inputAlias));
  dataSetSelecteds.push_back(std::make_pair(m_ui->m_dataSet2ComboBox->currentText().toUtf8().data(), m_ui->m_dataSetAliasLineEdit->text().toUtf8().data()));

  //property names
  std::set<std::string> propertyNames;

  //get properties for each data set
  for(size_t t = 0; t < dataSetSelecteds.size(); ++t)
  {
    //alias name
    std::string alias = dataSetSelecteds[t].second;

    //data set name
    std::string dataSetName = dataSetSelecteds[t].first;

    //get datasettype
    std::unique_ptr<te::da::DataSetType> dsType;

    for(unsigned int i = 0; i < datasetNames.size(); ++i)
    {
      if(datasetNames[i] == dataSetName)
      {
        dsType = m_ds->getDataSetType(datasetNames[i]);
        break;
      }
    }

    if(dsType.get())
    {
      //Acquiring the primary key's properties
      te::da::PrimaryKey* pk = dsType->getPrimaryKey();

      if(!pk)
        return;

      for(size_t i = 0; i < dsType->size(); ++i)
      {
        te::dt::Property* curProp = dsType->getProperty(i);
        std::string propName = curProp->getName();
        std::string fullName = alias + "." + propName;
        bool isNew = propertyNames.find(propName) == propertyNames.end();

        if(isNew)
          propertyNames.insert(propName);

        if (pk->has(dsType->getProperty(i)) || !isNew)
          m_uniqueProps.push_back(fullName);
        else
          m_properties.push_back(fullName);

        if(t == 0)
          m_ui->m_dataset1ColumnComboBox->addItem(QString::fromUtf8(fullName.c_str()), QVariant(dsType->getProperty(i)->getType()));
        else
        {
          m_ui->m_dataset2ColumnComboBox->addItem(QString::fromUtf8(fullName.c_str()), QVariant(dsType->getProperty(i)->getType()));
        }
      }
    }
  }

  if(index != -1)
    m_ui->m_dataset1ColumnComboBox->setCurrentIndex(index);
}

void te::qt::widgets::TableLinkDialog::done(int r)
{
  if(QDialog::Accepted == r)
  {
    QVariant dsv1, dsv2;
    dsv1 = m_ui->m_dataset1ColumnComboBox->itemData(m_ui->m_dataset1ColumnComboBox->currentIndex());
    dsv2 = m_ui->m_dataset2ColumnComboBox->itemData(m_ui->m_dataset2ColumnComboBox->currentIndex());
    std::string title = m_ui->m_layerTitleLineEdit->text().toUtf8().data();

     if(dsv1 != dsv2)
      {
        QMessageBox::warning(this, tr("Tabular File"), "The types of the selected columns do not match.");
        return;
      }
      else if(dsv1.toInt() != te::dt::STRING_TYPE && (dsv1.toInt() < te::dt::INT16_TYPE || dsv1.toInt() > te::dt::UINT64_TYPE))
      {
        QMessageBox::warning(this, tr("Tabular File"), "The types of the selected columns must be either an integer or a string.");
        return;
      }
      else if (title.empty())
      {
        QMessageBox::warning(this, tr("Tabular File"), "The new layer must have a title.");
        return;
      }
      else
      {
        QDialog::done(r);
        return;
      }
  }
  else
  {
      QDialog::done(r);
      return;
  }
}

int  te::qt::widgets::TableLinkDialog::exec()
{
  if (m_ds->getType() == "ADO")
  {
   QMessageBox::information(this, tr("Table link error"),
   tr("This function is not available for the selected datasource"));
   return QDialog::Rejected;
  }
  else if(!m_ds->getDataSetType(m_inputLayer->getDataSetName())->getPrimaryKey())
  {
    QMessageBox::information(this, tr("Table link error"),
                             tr("This function is not available for datasets without a primary key"));
    return QDialog::Rejected;
  }
  else
    return QDialog::exec();
}

void te::qt::widgets::TableLinkDialog::onDataCBIndexChanged(int index)
{
  std::string DsName = m_ui->m_dataSet2ComboBox->currentText().toUtf8().data();
  size_t pos = DsName.find(".");
  if(pos != std::string::npos)
    m_ui->m_dataSetAliasLineEdit->setText(QString::fromUtf8(DsName.substr(pos + 1, DsName.size() - 1).c_str()));
  else
    m_ui->m_dataSetAliasLineEdit->setText(QString::fromUtf8(DsName.c_str()));

  getProperties();
  m_ui->m_tabularFrame->hide();
  m_tabularView->hide();
  m_ui->m_dataPreviewGroupBox->hide();
}

void te::qt::widgets::TableLinkDialog::onDataToolButtonnClicked()
{
  std::string aux = m_ui->m_dataset2ColumnComboBox->currentText().toUtf8().data();
  std::string alias = "";
  size_t pos = aux.find(".");

  if (pos != std::string::npos)
    alias = aux.substr(0, pos);
  else
    alias = aux;

  //get datasettype
  std::unique_ptr<te::da::DataSetType> dsType;
  dsType = m_ds->getDataSetType(alias);

  //Get Dataset

  std::unique_ptr<te::da::DataSet> dataSet = m_ds->getDataSet(alias);
  
  //Acquiring the dataSet properties
  std::vector<std::size_t> dataSetProperties;

  for (size_t i = 0; i < dsType->size(); ++i)
  {
    dataSetProperties.push_back(i);
  }

  //Adjusting the table that will display the tabular dataset
  m_tabularView->setDataSet(new te::mem::DataSet(*dataSet.get(), dataSetProperties, 5));
  m_tabularView->resizeColumnsToContents();

  if(m_ui->m_tabularFrame->isHidden())
  {
    m_tabularView->show();
    m_ui->m_tabularFrame->show();
    m_ui->m_dataPreviewGroupBox->show();
  }
  else
  {
    m_ui->m_tabularFrame->hide();
    m_tabularView->hide();
    m_ui->m_dataPreviewGroupBox->hide();
  }
}

void te::qt::widgets::TableLinkDialog::onOkPushButtonClicked()
{
  this->accept();
}
