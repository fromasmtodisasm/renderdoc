/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <utility>
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "replay/replay_renderer.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "replay_proxy.h"
#include "socket_helpers.h"

using std::pair;

template <>
string ToStrHelper<false, CaptureOptions>::Get(const CaptureOptions &el)
{
  return "<...>";
}

enum RemoteServerPacket
{
  eRemoteServer_Noop,
  eRemoteServer_RemoteDriverList,
  eRemoteServer_TakeOwnershipCapture,
  eRemoteServer_CopyCapture,
  eRemoteServer_OpenLog,
  eRemoteServer_LogOpenProgress,
  eRemoteServer_LogOpened,
  eRemoteServer_CloseLog,
  eRemoteServer_ExecuteAndInject,
  eRemoteServer_RemoteServerCount,
};

RDCCOMPILE_ASSERT((int)eRemoteServer_RemoteServerCount < (int)eReplayProxy_First,
                  "Remote server and Replay Proxy packets overlap");

struct ProgressLoopData
{
  Network::Socket *sock;
  float progress;
  bool killsignal;
};

static void ProgressTicker(void *d)
{
  ProgressLoopData *data = (ProgressLoopData *)d;

  Serialiser ser("", Serialiser::WRITING, false);

  while(!data->killsignal)
  {
    ser.Rewind();
    ser.Serialise("", data->progress);

    if(!SendPacket(data->sock, eRemoteServer_LogOpenProgress, ser))
    {
      SAFE_DELETE(data->sock);
      break;
    }
    Threading::Sleep(100);
  }
}

void RenderDoc::BecomeRemoteServer(const char *listenhost, uint16_t port, volatile bool32 &killReplay)
{
  Network::Socket *sock = Network::CreateServerSocket(listenhost, port, 1);

  if(sock == NULL)
    return;

  std::vector<std::pair<uint32_t, uint32_t> > listenRanges;
  bool allowExecution = true;

  FILE *f = FileIO::fopen(FileIO::GetAppFolderFilename("remoteserver.conf").c_str(), "r");

  while(f && !FileIO::feof(f))
  {
    string line = trim(FileIO::getline(f));

    if(line == "")
      continue;

    // skip comments
    if(line[0] == '#')
      continue;

    if(line.substr(0, sizeof("whitelist") - 1) == "whitelist")
    {
      uint32_t ip = 0, mask = 0;

      // CIDR notation
      bool found = Network::ParseIPRangeCIDR(line.c_str() + sizeof("whitelist"), ip, mask);

      if(found)
      {
        listenRanges.push_back(std::make_pair(ip, mask));
        continue;
      }
      else
      {
        RDCLOG("Couldn't parse IP range from: %s", line.c_str() + sizeof("whitelist"));
      }

      continue;
    }
    else if(line.substr(0, sizeof("noexec") - 1) == "noexec")
    {
      allowExecution = false;

      continue;
    }

    RDCLOG("Malformed line '%s'. See documentation for file format.", line.c_str());
  }

  if(f)
    FileIO::fclose(f);

  if(listenRanges.empty())
  {
    RDCLOG("No whitelist IP ranges configured - using default private IP ranges.");
    RDCLOG(
        "Create a config file remoteserver.conf in ~/.renderdoc or %%APPDATA%%/renderdoc to narrow "
        "this down or accept connections from more ranges.");

    listenRanges.push_back(std::make_pair(Network::MakeIP(10, 0, 0, 0), 0xff000000));
    listenRanges.push_back(std::make_pair(Network::MakeIP(172, 16, 0, 0), 0xfff00000));
    listenRanges.push_back(std::make_pair(Network::MakeIP(192, 168, 0, 0), 0xffff0000));
  }

  RDCLOG("Allowing connections from:");

  for(size_t i = 0; i < listenRanges.size(); i++)
  {
    uint32_t ip = listenRanges[i].first;
    uint32_t mask = listenRanges[i].second;

    RDCLOG("%u.%u.%u.%u / %u.%u.%u.%u", Network::GetIPOctet(ip, 0), Network::GetIPOctet(ip, 1),
           Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3), Network::GetIPOctet(mask, 0),
           Network::GetIPOctet(mask, 1), Network::GetIPOctet(mask, 2), Network::GetIPOctet(mask, 3));
  }

  if(allowExecution)
    RDCLOG("Allowing execution commands");
  else
    RDCLOG("Blocking execution commands");

  RDCLOG("Replay host ready for requests...");

  while(!killReplay)
  {
    Network::Socket *client = sock->AcceptClient(false);

    if(client == NULL)
    {
      if(!sock->Connected())
      {
        RDCERR("Error in accept - shutting down server");

        SAFE_DELETE(sock);
        return;
      }

      Threading::Sleep(5);

      continue;
    }

    uint32_t ip = client->GetRemoteIP();

    RDCLOG("Connection received from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
           Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

    bool valid = false;

    // always allow connections from localhost
    valid = Network::MatchIPMask(ip, Network::MakeIP(127, 0, 0, 1), ~0U);

    for(size_t i = 0; i < listenRanges.size(); i++)
    {
      if(Network::MatchIPMask(ip, listenRanges[i].first, listenRanges[i].second))
      {
        valid = true;
        break;
      }
    }

    if(!valid)
    {
      RDCLOG("Doesn't match any listen range, closing connection.");
      SAFE_DELETE(client);
      continue;
    }

    vector<string> tempFiles;
    IRemoteDriver *driver = NULL;
    ProxySerialiser *proxy = NULL;

    Serialiser sendSer("", Serialiser::WRITING, false);

    while(client)
    {
      if(client && !client->Connected())
        break;

      if(killReplay)
        break;

      sendSer.Rewind();

      Threading::Sleep(4);

      if(client->IsRecvDataWaiting())
      {
        RemoteServerPacket type;
        Serialiser *recvser = NULL;

        if(!RecvPacket(client, type, &recvser))
          break;

        if(client == NULL)
        {
          SAFE_DELETE(recvser);
          continue;
        }
        else if(type == eRemoteServer_RemoteDriverList)
        {
          map<RDCDriver, string> drivers = RenderDoc::Inst().GetRemoteDrivers();

          uint32_t count = (uint32_t)drivers.size();
          sendSer.Serialise("", count);

          for(auto it = drivers.begin(); it != drivers.end(); ++it)
          {
            RDCDriver driver = it->first;
            sendSer.Serialise("", driver);
            sendSer.Serialise("", (*it).second);
          }

          type = eRemoteServer_RemoteDriverList;
        }
        else if(type == eRemoteServer_CopyCapture)
        {
          string cap_file;
          string dummy, dummy2;
          FileIO::GetDefaultFiles("remotecopy", cap_file, dummy, dummy2);

          Serialiser *fileRecv = NULL;

          RDCLOG("Copying file to local path '%s'.", cap_file.c_str());

          if(!RecvChunkedFile(client, type, cap_file.c_str(), fileRecv, NULL))
          {
            FileIO::Delete(cap_file.c_str());

            RDCERR("Network error receiving file");

            SAFE_DELETE(fileRecv);
            break;
          }

          RDCLOG("File received.");

          tempFiles.push_back(cap_file);

          SAFE_DELETE(fileRecv);
        }
        else if(type == eRemoteServer_TakeOwnershipCapture)
        {
          string cap_file;
          recvser->Serialise("filename", cap_file);

          RDCLOG("Taking ownership of '%s'.", cap_file.c_str());

          tempFiles.push_back(cap_file);
        }
        else if(type == eRemoteServer_OpenLog)
        {
          string cap_file;
          recvser->Serialise("filename", cap_file);

          RDCASSERT(driver == NULL && proxy == NULL);

          RDCDriver driverType = RDC_Unknown;
          string driverName = "";
          RenderDoc::Inst().FillInitParams(cap_file.c_str(), driverType, driverName, NULL);

          if(RenderDoc::Inst().HasRemoteDriver(driverType))
          {
            ProgressLoopData data;

            data.sock = client;
            data.killsignal = false;
            data.progress = 0.0f;

            RenderDoc::Inst().SetProgressPtr(&data.progress);

            Threading::ThreadHandle ticker = Threading::CreateThread(ProgressTicker, &data);

            ReplayCreateStatus status =
                RenderDoc::Inst().CreateRemoteDriver(driverType, cap_file.c_str(), &driver);

            sendSer.Serialise("status", status);

            if(status != eReplayCreate_Success || driver == NULL)
            {
              RDCERR("Failed to create remote driver for driver type %d name %s", driverType,
                     driverName.c_str());
              continue;
            }

            driver->ReadLogInitialisation();

            RenderDoc::Inst().SetProgressPtr(NULL);

            data.killsignal = true;
            Threading::JoinThread(ticker);
            Threading::CloseThread(ticker);

            type = eRemoteServer_LogOpened;

            proxy = new ProxySerialiser(client, driver);
          }
          else
          {
            RDCERR("File needs driver for %s which isn't supported!", driverName.c_str());

            ReplayCreateStatus status = eReplayCreate_APIUnsupported;
            sendSer.Serialise("status", status);

            type = eRemoteServer_LogOpened;
          }
        }
        else if(type == eRemoteServer_CloseLog)
        {
          if(driver)
            driver->Shutdown();
          driver = NULL;

          SAFE_DELETE(proxy);

          type = eRemoteServer_Noop;
        }
        else if(type == eRemoteServer_ExecuteAndInject)
        {
          string app, workingDir, cmdLine, logfile;
          CaptureOptions opts;
          recvser->Serialise("app", app);
          recvser->Serialise("workingDir", workingDir);
          recvser->Serialise("cmdLine", cmdLine);
          recvser->Serialise("logfile", logfile);
          recvser->Serialise("opts", opts);

          uint32_t ident = Process::LaunchAndInjectIntoProcess(
              app.c_str(), workingDir.c_str(), cmdLine.c_str(), logfile.c_str(), &opts, false);

          sendSer.Serialise("ident", ident);

          type = eRemoteServer_ExecuteAndInject;
        }
        else if((int)type >= eReplayProxy_First && proxy)
        {
          bool ok = proxy->Tick(type, recvser);

          if(!ok)
            break;

          continue;
        }

        SAFE_DELETE(recvser);

        if(type != eRemoteServer_Noop && !SendPacket(client, type, sendSer))
        {
          RDCERR("Network error sending supported driver list");
          break;
        }

        continue;
      }
    }

    SAFE_DELETE(client);

    if(driver)
      driver->Shutdown();
    SAFE_DELETE(proxy);

    for(size_t i = 0; i < tempFiles.size(); i++)
    {
      FileIO::Delete(tempFiles[i].c_str());
    }

    RDCLOG("Closing replay connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
           Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

    RDCLOG("Ready for new connection...");
  }

  SAFE_DELETE(sock);
}

struct RemoteServer : public IRemoteServer
{
public:
  RemoteServer(Network::Socket *sock) : m_Socket(sock)
  {
    map<RDCDriver, string> m = RenderDoc::Inst().GetReplayDrivers();

    m_Proxies.reserve(m.size());
    for(auto it = m.begin(); it != m.end(); ++it)
      m_Proxies.push_back(*it);
  }
  virtual ~RemoteServer() { SAFE_DELETE(m_Socket); }
  void Shutdown() { delete this; }
  bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
  bool LocalProxies(rdctype::array<rdctype::str> *out)
  {
    if(out == NULL)
      return false;

    create_array_uninit(*out, m_Proxies.size());

    size_t i = 0;
    for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
      out->elems[i] = it->second;

    return true;
  }

  bool RemoteSupportedReplays(rdctype::array<rdctype::str> *out)
  {
    if(out == NULL)
      return false;

    {
      Serialiser sendData("", Serialiser::WRITING, false);
      Send(eRemoteServer_RemoteDriverList, sendData);

      RemoteServerPacket type = eRemoteServer_RemoteDriverList;

      Serialiser *ser = NULL;
      Get(type, &ser);

      if(ser)
      {
        uint32_t count = 0;
        ser->Serialise("", count);

        create_array_uninit(*out, count);

        for(uint32_t i = 0; i < count; i++)
        {
          RDCDriver driver = RDC_Unknown;
          string name = "";
          ser->Serialise("", driver);
          ser->Serialise("", name);

          out->elems[i] = name;
        }

        delete ser;
      }
    }

    return true;
  }

  uint32_t ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                            const char *logfile, const CaptureOptions *opts)
  {
    CaptureOptions capopts = opts ? *opts : CaptureOptions();

    string appstr = app && app[0] ? app : "";
    string workstr = workingDir && workingDir[0] ? workingDir : "";
    string cmdstr = cmdLine && cmdLine[0] ? cmdLine : "";
    string logstr = logfile && logfile[0] ? logfile : "";

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("app", appstr);
    sendData.Serialise("workingDir", workstr);
    sendData.Serialise("cmdLine", cmdstr);
    sendData.Serialise("logfile", logstr);
    sendData.Serialise("opts", capopts);
    Send(eRemoteServer_ExecuteAndInject, sendData);

    RemoteServerPacket type = eRemoteServer_ExecuteAndInject;

    Serialiser *ser = NULL;
    Get(type, &ser);

    uint32_t ident = 0;

    if(ser)
      ser->Serialise("ident", ident);

    SAFE_DELETE(ser);

    return ident;
  }

  void CopyCapture(const char *filename, float *progress)
  {
    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_CopyCapture, sendData);

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    sendData.Rewind();

    if(!SendChunkedFile(m_Socket, eRemoteServer_CopyCapture, filename, sendData, progress))
    {
      SAFE_DELETE(m_Socket);
      return;
    }
  }

  void TakeOwnershipCapture(const char *filename)
  {
    string logfile = filename;

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("logfile", logfile);
    Send(eRemoteServer_TakeOwnershipCapture, sendData);
  }

  ReplayCreateStatus OpenCapture(uint32_t proxyid, const char *filename, float *progress,
                                 ReplayRenderer **rend)
  {
    if(rend == NULL)
      return eReplayCreate_InternalError;

    string logfile = filename;

    if(proxyid >= m_Proxies.size())
    {
      RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
      return eReplayCreate_InternalError;
    }

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    RDCDriver proxydrivertype = m_Proxies[proxyid].first;

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("logfile", logfile);
    Send(eRemoteServer_OpenLog, sendData);

    Serialiser *progressSer = NULL;
    RemoteServerPacket type = eRemoteServer_Noop;
    while(m_Socket)
    {
      Get(type, &progressSer);

      if(!m_Socket || progressSer == NULL || type != eRemoteServer_LogOpenProgress)
        break;

      progressSer->Serialise("", *progress);

      RDCLOG("% 3.0f%%...", (*progress) * 100.0f);

      SAFE_DELETE(progressSer);
    }

    if(!m_Socket || progressSer == NULL || type != eRemoteServer_LogOpened)
      return eReplayCreate_NetworkIOFailed;

    ReplayCreateStatus status = eReplayCreate_Success;
    progressSer->Serialise("status", status);

    SAFE_DELETE(progressSer);

    *progress = 1.0f;

    if(status != eReplayCreate_Success)
      return status;

    RDCLOG("Log ready on replay host");

    IReplayDriver *proxyDriver = NULL;
    status = RenderDoc::Inst().CreateReplayDriver(proxydrivertype, NULL, &proxyDriver);

    if(status != eReplayCreate_Success || !proxyDriver)
    {
      if(proxyDriver)
        proxyDriver->Shutdown();
      return status;
    }

    ReplayRenderer *ret = new ReplayRenderer();

    ProxySerialiser *proxy = new ProxySerialiser(m_Socket, proxyDriver);
    status = ret->SetDevice(proxy);

    if(status != eReplayCreate_Success)
    {
      SAFE_DELETE(ret);
      return status;
    }

    // ReplayRenderer takes ownership of the ProxySerialiser (as IReplayDriver)
    // and it cleans itself up in Shutdown.

    *rend = ret;

    return eReplayCreate_Success;
  }

  void CloseCapture(ReplayRenderer *rend)
  {
    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_CloseLog, sendData);

    rend->Shutdown();
  }

private:
  Network::Socket *m_Socket;

  void Send(RemoteServerPacket type, const Serialiser &ser) { SendPacket(m_Socket, type, ser); }
  void Get(RemoteServerPacket &type, Serialiser **ser)
  {
    vector<byte> payload;

    if(!RecvPacket(m_Socket, type, payload))
    {
      SAFE_DELETE(m_Socket);
      if(ser)
        *ser = NULL;
      return;
    }

    if(ser)
      *ser = new Serialiser(payload.size(), &payload[0], false);
  }

  vector<pair<RDCDriver, string> > m_Proxies;
};

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_Shutdown(RemoteServer *remote)
{
  remote->Shutdown();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteServer_LocalProxies(RemoteServer *remote, rdctype::array<rdctype::str> *out)
{
  return remote->LocalProxies(out);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteServer_RemoteSupportedReplays(RemoteServer *remote, rdctype::array<rdctype::str> *out)
{
  return remote->RemoteSupportedReplays(out);
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RemoteServer_ExecuteAndInject(RemoteServer *remote, const char *app, const char *workingDir,
                              const char *cmdLine, const char *logfile, const CaptureOptions *opts)
{
  return remote->ExecuteAndInject(app, workingDir, cmdLine, logfile, opts);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_TakeOwnershipCapture(RemoteServer *remote,
                                                                             const char *filename)
{
  return remote->TakeOwnershipCapture(filename);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CopyCapture(RemoteServer *remote,
                                                                    const char *filename,
                                                                    float *progress)
{
  return remote->CopyCapture(filename, progress);
}

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RemoteServer_OpenCapture(RemoteServer *remote, uint32_t proxyid, const char *logfile,
                         float *progress, ReplayRenderer **rend)
{
  return remote->OpenCapture(proxyid, logfile, progress, rend);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CloseCapture(RemoteServer *remote,
                                                                     ReplayRenderer *rend)
{
  return remote->CloseCapture(rend);
}

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *host, uint32_t port, RemoteServer **rend)
{
  if(rend == NULL)
    return eReplayCreate_InternalError;

  string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  if(port == 0)
    port = RENDERDOC_GetDefaultRemoteServerPort();

  Network::Socket *sock = NULL;

  if(s != "-")
  {
    sock = Network::CreateClientSocket(s.c_str(), (uint16_t)port, 3000);

    if(sock == NULL)
      return eReplayCreate_NetworkIOFailed;
  }

  *rend = new RemoteServer(sock);

  return eReplayCreate_Success;
}