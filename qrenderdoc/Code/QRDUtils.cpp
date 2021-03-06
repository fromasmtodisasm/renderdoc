/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "QRDUtils.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMenu>
#include <QMetaMethod>

QString ToQStr(const ResourceUsage usage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(usage)
    {
      case eUsage_VertexBuffer: return "Vertex Buffer";
      case eUsage_IndexBuffer: return "Index Buffer";

      case eUsage_VS_Constants: return "VS - Constant Buffer";
      case eUsage_GS_Constants: return "GS - Constant Buffer";
      case eUsage_HS_Constants: return "HS - Constant Buffer";
      case eUsage_DS_Constants: return "DS - Constant Buffer";
      case eUsage_CS_Constants: return "CS - Constant Buffer";
      case eUsage_PS_Constants: return "PS - Constant Buffer";
      case eUsage_All_Constants: return "All - Constant Buffer";

      case eUsage_SO: return "Stream Out";

      case eUsage_VS_Resource: return "VS - Resource";
      case eUsage_GS_Resource: return "GS - Resource";
      case eUsage_HS_Resource: return "HS - Resource";
      case eUsage_DS_Resource: return "DS - Resource";
      case eUsage_CS_Resource: return "CS - Resource";
      case eUsage_PS_Resource: return "PS - Resource";
      case eUsage_All_Resource: return "All - Resource";

      case eUsage_VS_RWResource: return "VS - UAV";
      case eUsage_HS_RWResource: return "HS - UAV";
      case eUsage_DS_RWResource: return "DS - UAV";
      case eUsage_GS_RWResource: return "GS - UAV";
      case eUsage_PS_RWResource: return "PS - UAV";
      case eUsage_CS_RWResource: return "CS - UAV";
      case eUsage_All_RWResource: return "All - UAV";

      case eUsage_InputTarget: return "Colour Input";
      case eUsage_ColourTarget: return "Rendertarget";
      case eUsage_DepthStencilTarget: return "Depthstencil";

      case eUsage_Indirect: return "Indirect argument";

      case eUsage_Clear: return "Clear";

      case eUsage_GenMips: return "Generate Mips";
      case eUsage_Resolve: return "Resolve";
      case eUsage_ResolveSrc: return "Resolve - Source";
      case eUsage_ResolveDst: return "Resolve - Dest";
      case eUsage_Copy: return "Copy";
      case eUsage_CopySrc: return "Copy - Source";
      case eUsage_CopyDst: return "Copy - Dest";

      case eUsage_Barrier: return "Barrier";
      default: break;
    }
  }
  else if(apitype == eGraphicsAPI_OpenGL || apitype == eGraphicsAPI_Vulkan)
  {
    const bool vk = (apitype == eGraphicsAPI_Vulkan);

    switch(usage)
    {
      case eUsage_VertexBuffer: return "Vertex Buffer";
      case eUsage_IndexBuffer: return "Index Buffer";

      case eUsage_VS_Constants: return "VS - Uniform Buffer";
      case eUsage_GS_Constants: return "GS - Uniform Buffer";
      case eUsage_HS_Constants: return "HS - Uniform Buffer";
      case eUsage_DS_Constants: return "DS - Uniform Buffer";
      case eUsage_CS_Constants: return "CS - Uniform Buffer";
      case eUsage_PS_Constants: return "PS - Uniform Buffer";
      case eUsage_All_Constants: return "All - Uniform Buffer";

      case eUsage_SO: return "Transform Feedback";

      case eUsage_VS_Resource: return "VS - Texture";
      case eUsage_GS_Resource: return "GS - Texture";
      case eUsage_HS_Resource: return "HS - Texture";
      case eUsage_DS_Resource: return "DS - Texture";
      case eUsage_CS_Resource: return "CS - Texture";
      case eUsage_PS_Resource: return "PS - Texture";
      case eUsage_All_Resource: return "All - Texture";

      case eUsage_VS_RWResource: return "VS - Image/SSBO";
      case eUsage_HS_RWResource: return "HS - Image/SSBO";
      case eUsage_DS_RWResource: return "DS - Image/SSBO";
      case eUsage_GS_RWResource: return "GS - Image/SSBO";
      case eUsage_PS_RWResource: return "PS - Image/SSBO";
      case eUsage_CS_RWResource: return "CS - Image/SSBO";
      case eUsage_All_RWResource: return "All - Image/SSBO";

      case eUsage_InputTarget: return "FBO Input";
      case eUsage_ColourTarget: return "FBO Colour";
      case eUsage_DepthStencilTarget: return "FBO Depthstencil";

      case eUsage_Indirect: return "Indirect argument";

      case eUsage_Clear: return "Clear";

      case eUsage_GenMips: return "Generate Mips";
      case eUsage_Resolve: return vk ? "Resolve" : "Framebuffer blit";
      case eUsage_ResolveSrc: return vk ? "Resolve - Source" : "Framebuffer blit - Source";
      case eUsage_ResolveDst: return vk ? "Resolve - Dest" : "Framebuffer blit - Dest";
      case eUsage_Copy: return "Copy";
      case eUsage_CopySrc: return "Copy - Source";
      case eUsage_CopyDst: return "Copy - Dest";

      case eUsage_Barrier: return "Barrier";
      default: break;
    }
  }

  return "Unknown";
}

QString ToQStr(const ShaderStageType stage, const GraphicsAPI apitype)
{
  if(IsD3D(apitype))
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "Vertex";
      case eShaderStage_Hull: return "Hull";
      case eShaderStage_Domain: return "Domain";
      case eShaderStage_Geometry: return "Geometry";
      case eShaderStage_Pixel: return "Pixel";
      case eShaderStage_Compute: return "Compute";
      default: break;
    }
  }
  else if(apitype == eGraphicsAPI_OpenGL || apitype == eGraphicsAPI_Vulkan)
  {
    switch(stage)
    {
      case eShaderStage_Vertex: return "Vertex";
      case eShaderStage_Tess_Control: return "Tess. Control";
      case eShaderStage_Tess_Eval: return "Tess. Eval";
      case eShaderStage_Geometry: return "Geometry";
      case eShaderStage_Fragment: return "Fragment";
      case eShaderStage_Compute: return "Compute";
      default: break;
    }
  }

  return "Unknown";
}

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  // marker that this data is valid
  data[magicIdentifier] = magicVersion;

  QJsonDocument doc = QJsonDocument::fromVariant(data);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert data to JSON document";
    return false;
  }

  QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

  qint64 ret = f.write(jsontext);

  if(ret != jsontext.size())
  {
    qCritical() << "Failed to write JSON data: " << ret << " " << f.errorString();
    return false;
  }

  return true;
}

bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  QByteArray json = f.readAll();

  if(json.isEmpty())
  {
    qCritical() << "Read invalid empty JSON data from file " << f.errorString();
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(json);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert file to JSON document";
    return false;
  }

  data = doc.toVariant().toMap();

  if(data.isEmpty() || !data.contains(magicIdentifier))
  {
    qCritical() << "Converted config data is invalid or unrecognised";
    return false;
  }

  if(data[magicIdentifier].toUInt() != magicVersion)
  {
    qCritical() << "Converted config data is not the right version";
    return false;
  }

  return true;
}

int GUIInvoke::methodIndex = -1;

void GUIInvoke::init()
{
  GUIInvoke *invoke = new GUIInvoke();
  methodIndex = invoke->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("doInvoke()"));
}

void GUIInvoke::call(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::QueuedConnection);
}

void GUIInvoke::blockcall(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  invoke->metaObject()->method(methodIndex).invoke(invoke, Qt::BlockingQueuedConnection);
}

const QMessageBox::StandardButtons RDDialog::YesNoCancel =
    QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

void RDDialog::show(QMenu *menu, QPoint pos)
{
  menu->setWindowModality(Qt::ApplicationModal);
  menu->popup(pos);
  QEventLoop loop;
  while(menu->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }
}

int RDDialog::show(QDialog *dialog)
{
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->show();
  QEventLoop loop;
  while(dialog->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }

  return dialog->result();
}

QMessageBox::StandardButton RDDialog::messageBox(QMessageBox::Icon icon, QWidget *parent,
                                                 const QString &title, const QString &text,
                                                 QMessageBox::StandardButtons buttons,
                                                 QMessageBox::StandardButton defaultButton)
{
  QMessageBox::StandardButton ret = defaultButton;

  // if we're already on the right thread, this boils down to a function call
  GUIInvoke::blockcall([&]() {
    QMessageBox mb(icon, title, text, buttons, parent);
    mb.setDefaultButton(defaultButton);
    show(&mb);
    ret = mb.standardButton(mb.clickedButton());
  });
  return ret;
}

QString RDDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                       QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, QString());
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::DirectoryOnly);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptSave);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}
