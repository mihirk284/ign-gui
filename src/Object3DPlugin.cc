/*
 * Copyright (C) 2017 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <ignition/common/Console.hh>
#include <ignition/rendering.hh>

#include "ignition/gui/CollapsibleWidget.hh"
#include "ignition/gui/ColorWidget.hh"
#include "ignition/gui/NumberWidget.hh"
#include "ignition/gui/Pose3dWidget.hh"
#include "ignition/gui/QtMetatypes.hh"
#include "ignition/gui/Object3DPlugin.hh"

// Default pose
static const ignition::math::Pose3d kDefaultPose{ignition::math::Pose3d::Zero};

// Default color
static const ignition::math::Color kDefaultColor{
    ignition::math::Color(0.7, 0.7, 0.7, 1.0)};

namespace ignition
{
namespace gui
{
  /// \brief Holds configuration for an object
  struct ObjInfo
  {
    /// \brief Pose in the world
    math::Pose3d pose{kDefaultPose};

    /// \brief Color
    math::Color color{kDefaultColor};
  };

  class Object3DPluginPrivate
  {
  };
}
}

using namespace ignition;
using namespace gui;

/////////////////////////////////////////////////
Object3DPlugin::Object3DPlugin()
  : Plugin(), dataPtr(new Object3DPluginPrivate)
{
}

/////////////////////////////////////////////////
Object3DPlugin::~Object3DPlugin()
{
}

/////////////////////////////////////////////////
void Object3DPlugin::LoadConfig(const tinyxml2::XMLElement *_pluginElem)
{
  if (this->title.empty())
    this->title = "3D " + this->typeSingular;

  // Configuration
  std::string engineName{"ogre"};
  std::vector<ObjInfo> objInfos;
  if (_pluginElem)
  {
    // All objs managed belong to the same engine and scene
    if (auto elem = _pluginElem->FirstChildElement("engine"))
      engineName = elem->GetText();

    if (auto elem = _pluginElem->FirstChildElement("scene"))
      this->sceneName = elem->GetText();

    // For objs to be inserted at startup
    for (auto insertElem = _pluginElem->FirstChildElement("insert");
         insertElem != nullptr;
        insertElem = insertElem->NextSiblingElement("insert"))
    {
      ObjInfo objInfo;

      if (auto elem = insertElem->FirstChildElement("pose"))
      {
        std::stringstream poseStr;
        poseStr << std::string(elem->GetText());
        poseStr >> objInfo.pose;
      }

      if (auto elem = insertElem->FirstChildElement("color"))
      {
        std::stringstream colorStr;
        colorStr << std::string(elem->GetText());
        colorStr >> objInfo.color;
      }

      objInfos.push_back(objInfo);
    }
  }

  // Keep error to show the user
  std::string error{""};
  rendering::ScenePtr scene;

  // Render engine
  this->engine = rendering::engine(engineName);
  if (!this->engine)
  {
    error = "Engine \"" + engineName
           + "\" not supported, plugin won't work.";
    ignwarn << error << std::endl;
  }
  else
  {
    // Scene
    scene = this->engine->SceneByName(this->sceneName);
    if (!scene)
    {
      error = "Scene \"" + this->sceneName
             + "\" not found, plugin won't work.";
      ignwarn << error << std::endl;
    }
    else
    {
      auto root = scene->RootVisual();

      // Initial objs
      for (const auto &o : objInfos)
      {
    //    auto grid = scene->CreateObject();
    //    grid->SetCellCount(o.cellCount);
    //    grid->SetVerticalCellCount(o.vertCellCount);
    //    grid->SetCellLength(o.cellLength);
    //
    //    auto gridVis = scene->CreateVisual();
    //    root->AddChild(gridVis);
    //    gridVis->SetLocalPose(o.pose);
    //    gridVis->AddGeometry(grid);
    //
    //    auto mat = scene->CreateMaterial();
    //    mat->SetAmbient(o.color);
    //    gridVis->SetMaterial(mat);
      }
    }
  }

  // Don't waste time loading widgets if this will be deleted anyway
  if (this->DeleteLaterRequested())
    return;

  if (!error.empty())
  {
    // Add message
    auto msg = new QLabel(QString::fromStdString(error));

    auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(msg);
    mainLayout->setAlignment(msg, Qt::AlignCenter);
    this->setLayout(mainLayout);

    return;
  }


  this->OnRefresh();
}

/////////////////////////////////////////////////
void Object3DPlugin::OnRefresh()
{
  auto mainLayout = this->layout();

  // Clear previous layout
  if (mainLayout)
  {
    while (mainLayout->count() != 1)
    {
      auto item = mainLayout->takeAt(1);
      if (qobject_cast<CollapsibleWidget *>(item->widget()))
      {
        delete item->widget();
        delete item;
      }
    }
  }
  // Creating layout for the first time
  else
  {
    mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    this->setLayout(mainLayout);

    auto addButton = new QPushButton("New " +
        QString::fromStdString(this->typeSingular));
    addButton->setObjectName("addButton" + QString::fromStdString(
        this->typeSingular));
    addButton->setToolTip("Add a new " + QString::fromStdString(
        this->typeSingular) + " with default values");
    this->connect(addButton, SIGNAL(clicked()), this, SLOT(OnAdd()));

    auto refreshButton = new QPushButton("Refresh");
    refreshButton->setObjectName("refreshButton" + QString::fromStdString(
        this->typeSingular));
    refreshButton->setToolTip("Refresh the list of objs");
    this->connect(refreshButton, SIGNAL(clicked()), this, SLOT(OnRefresh()));

    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(refreshButton);

    auto buttonsWidget = new QWidget();
    buttonsWidget->setLayout(buttonsLayout);

    mainLayout->addWidget(buttonsWidget);
  }

  auto scene = this->engine->SceneByName(this->sceneName);

  // Scene has been destroyed
  if (!scene)
  {
    // Delete buttons
    auto item = mainLayout->takeAt(0);
    if (item)
    {
      delete item->widget();
      delete item;
    }

    // Add message
    auto msg = new QLabel(QString::fromStdString(
        "Scene \"" + this->sceneName + "\" has been destroyed.\n"
        + "Create a new scene and then open a new plugin."));
    mainLayout->addWidget(msg);
    mainLayout->setAlignment(msg, Qt::AlignCenter);
    return;
  }

  // Clear current list of objects
  this->objs.clear();

  // Update list
  this->Refresh();

  auto spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainLayout->addWidget(spacer);
}

/////////////////////////////////////////////////
void Object3DPlugin::AppendObj(const rendering::ObjectPtr &_obj,
    const std::vector<PropertyWidget *> _props)
{
  // Store on list
  this->objs.push_back(_obj);

  // Delete button
  auto objName = QString::fromStdString(_obj->Name());
  auto deleteButton = new QPushButton("Delete " +
      QString::fromStdString(this->typeSingular));
  deleteButton->setToolTip("Delete " +
      QString::fromStdString(this->typeSingular) + " " + objName);
  deleteButton->setProperty("objName", objName);
  deleteButton->setObjectName("deleteButton");
  this->connect(deleteButton, SIGNAL(clicked()), this, SLOT(OnDelete()));

  // Collapsible
  auto collapsible = new CollapsibleWidget(_obj->Name());
  for (const auto &prop : _props)
    collapsible->AppendContent(prop);
  collapsible->AppendContent(deleteButton);

  // Add to layout
  this->layout()->addWidget(collapsible);
}

/////////////////////////////////////////////////
void Object3DPlugin::OnChange(const QVariant &_value)
{
  auto objName = this->sender()->property("objName").toString().toStdString();
  auto type = this->sender()->objectName().toStdString();

  for (auto obj : this->objs)
  {
    if (obj->Name() != objName)
      continue;

    this->Change(obj, type, _value);

    break;
  }
}

/////////////////////////////////////////////////
void Object3DPlugin::OnDelete()
{
  auto objName = this->sender()->property("objName").toString().toStdString();
  auto type = this->sender()->objectName().toStdString();

  for (auto obj : this->objs)
  {
    if (obj->Name() != objName)
      continue;

    if (!this->Delete(obj))
      continue;

    this->objs.erase(std::remove(this->objs.begin(),
                                 this->objs.end(), obj),
                                 this->objs.end());

    this->OnRefresh();
    break;
  }
}

/////////////////////////////////////////////////
void Object3DPlugin::OnAdd()
{
  this->Add();

  this->OnRefresh();
}