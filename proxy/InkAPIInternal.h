/** @file

  Internal SDK stuff

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#pragma once

#include "P_EventSystem.h"
#include "URL.h"
#include "P_Net.h"
#include "HTTP.h"
#include "tscore/List.h"
#include "ProxyConfig.h"
#include "P_Cache.h"
#include "I_Tasks.h"

#include "ts/InkAPIPrivateIOCore.h"
#include "ts/experimental.h"

#include <typeinfo>

/* Some defines that might be candidates for configurable settings later.
 */
#define TS_HTTP_MAX_USER_ARG 16 /* max number of user arguments for Transactions and Sessions */

typedef int8_t TSMgmtByte; // Not for external use

/* ****** Cache Structure ********* */

// For memory corruption detection
enum CacheInfoMagic {
  CACHE_INFO_MAGIC_ALIVE = 0xfeedbabe,
  CACHE_INFO_MAGIC_DEAD  = 0xdeadbeef,
};

struct CacheInfo {
  CryptoHash cache_key;
  CacheFragType frag_type;
  char *hostname;
  int len;
  time_t pin_in_cache;
  CacheInfoMagic magic;

  CacheInfo()
  {
    frag_type    = CACHE_FRAG_TYPE_NONE;
    hostname     = nullptr;
    len          = 0;
    pin_in_cache = 0;
    magic        = CACHE_INFO_MAGIC_ALIVE;
  }
};

class FileImpl
{
  enum {
    CLOSED = 0,
    READ   = 1,
    WRITE  = 2,
  };

public:
  FileImpl();
  ~FileImpl();

  int fopen(const char *filename, const char *mode);
  void fclose();
  ssize_t fread(void *buf, size_t length);
  ssize_t fwrite(const void *buf, size_t length);
  ssize_t fflush();
  char *fgets(char *buf, size_t length);

public:
  int m_fd;
  int m_mode;
  char *m_buf;
  size_t m_bufsize;
  size_t m_bufpos;
};

struct INKConfigImpl : public ConfigInfo {
  void *mdata;
  TSConfigDestroyFunc m_destroy_func;

  ~INKConfigImpl() override { m_destroy_func(mdata); }
};

struct HttpAltInfo {
  HTTPHdr m_client_req;
  HTTPHdr m_cached_req;
  HTTPHdr m_cached_resp;
  float m_qvalue;
};

enum APIHookScope {
  API_HOOK_SCOPE_NONE,
  API_HOOK_SCOPE_GLOBAL,
  API_HOOK_SCOPE_LOCAL,
};

/// A single API hook that can be invoked.
class APIHook
{
public:
  INKContInternal *m_cont;
  int invoke(int event, void *edata);
  APIHook *next() const;
  LINK(APIHook, m_link);
};

/// A collection of API hooks.
class APIHooks
{
public:
  void prepend(INKContInternal *cont);
  void append(INKContInternal *cont);
  APIHook *get() const;
  void clear();
  bool is_empty() const;

private:
  Que(APIHook, m_link) m_hooks;
};

inline bool
APIHooks::is_empty() const
{
  return nullptr == m_hooks.head;
}

/** Container for API hooks for a specific feature.

    This is an array of hook lists, each identified by a numeric identifier (id). Each array element is a list of all
    hooks for that ID. Adding a hook means adding to the list in the corresponding array element. There is no provision
    for removing a hook.

    @note The minimum value for a hook ID is zero. Therefore the template parameter @a N_ID should be one more than the
    maximum hook ID so the valid ids are 0..(N-1) in the standard C array style.
 */
template <typename ID, ///< Type of hook ID
          int N        ///< Number of hooks
          >
class FeatureAPIHooks
{
public:
  FeatureAPIHooks();  ///< Constructor (empty container).
  ~FeatureAPIHooks(); ///< Destructor.

  /// Remove all hooks.
  void clear();
  /// Add the hook @a cont to the front of the hooks for @a id.
  void prepend(ID id, INKContInternal *cont);
  /// Add the hook @a cont to the end of the hooks for @a id.
  void append(ID id, INKContInternal *cont);
  /// Get the list of hooks for @a id.
  APIHook *get(ID id) const;
  /// @return @c true if @a id is a valid id, @c false otherwise.
  static bool is_valid(ID id);

  /// Invoke the callbacks for the hook @a id.
  void invoke(ID id, int event, void *data);

  /// Fast check for any hooks in this container.
  ///
  /// @return @c true if any list has at least one hook, @c false if
  /// all lists have no hooks.
  bool has_hooks() const;

  /// Check for existence of hooks of a specific @a id.
  /// @return @c true if any hooks of type @a id are present.
  bool has_hooks_for(ID id) const;

private:
  bool hooks_p = false; ///< Flag for (not) empty container.
  /// The array of hooks lists.
  APIHooks m_hooks[N];
};

template <typename ID, int N> FeatureAPIHooks<ID, N>::FeatureAPIHooks() {}

template <typename ID, int N> FeatureAPIHooks<ID, N>::~FeatureAPIHooks()
{
  this->clear();
}

template <typename ID, int N>
void
FeatureAPIHooks<ID, N>::clear()
{
  for (int i = 0; i < N; ++i) {
    m_hooks[i].clear();
  }
  hooks_p = false;
}

template <typename ID, int N>
void
FeatureAPIHooks<ID, N>::prepend(ID id, INKContInternal *cont)
{
  if (likely(is_valid(id))) {
    hooks_p = true;
    m_hooks[id].prepend(cont);
  }
}

template <typename ID, int N>
void
FeatureAPIHooks<ID, N>::append(ID id, INKContInternal *cont)
{
  if (likely(is_valid(id))) {
    hooks_p = true;
    m_hooks[id].append(cont);
  }
}

template <typename ID, int N>
APIHook *
FeatureAPIHooks<ID, N>::get(ID id) const
{
  return likely(is_valid(id)) ? m_hooks[id].get() : nullptr;
}

template <typename ID, int N>
void
FeatureAPIHooks<ID, N>::invoke(ID id, int event, void *data)
{
  if (likely(is_valid(id))) {
    m_hooks[id].invoke(event, data);
  }
}

template <typename ID, int N>
bool
FeatureAPIHooks<ID, N>::has_hooks() const
{
  return hooks_p;
}

template <typename ID, int N>
bool
FeatureAPIHooks<ID, N>::is_valid(ID id)
{
  return 0 <= id && id < N;
}

class HttpAPIHooks : public FeatureAPIHooks<TSHttpHookID, TS_HTTP_LAST_HOOK>
{
};

class TSSslHookInternalID
{
public:
  constexpr TSSslHookInternalID(TSHttpHookID id) : _id(id - TS_SSL_FIRST_HOOK) {}

  constexpr operator int() const { return _id; }

  static const int NUM = TS_SSL_LAST_HOOK - TS_SSL_FIRST_HOOK + 1;

  constexpr bool
  is_in_bounds() const
  {
    return (_id >= 0) && (_id < NUM);
  }

private:
  const int _id;
};

class SslAPIHooks : public FeatureAPIHooks<TSSslHookInternalID, TSSslHookInternalID::NUM>
{
};

class LifecycleAPIHooks : public FeatureAPIHooks<TSLifecycleHookID, TS_LIFECYCLE_LAST_HOOK>
{
};

class ConfigUpdateCallback : public Continuation
{
public:
  ConfigUpdateCallback(INKContInternal *contp) : Continuation(contp->mutex.get()), m_cont(contp)
  {
    SET_HANDLER(&ConfigUpdateCallback::event_handler);
  }

  int
  event_handler(int, void *)
  {
    if (m_cont->mutex) {
      MUTEX_TRY_LOCK(trylock, m_cont->mutex, this_ethread());
      if (!trylock.is_locked()) {
        eventProcessor.schedule_in(this, HRTIME_MSECONDS(10), ET_TASK);
      } else {
        m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, nullptr);
        delete this;
      }
    } else {
      m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, nullptr);
      delete this;
    }

    return 0;
  }

private:
  INKContInternal *m_cont;
};

class ConfigUpdateCbTable
{
public:
  ConfigUpdateCbTable();
  ~ConfigUpdateCbTable();

  void insert(INKContInternal *contp, const char *name);
  void invoke(const char *name);
  void invoke(INKContInternal *contp);

private:
  std::unordered_map<std::string, INKContInternal *> cb_table;
};

void api_init();

extern HttpAPIHooks *http_global_hooks;
extern LifecycleAPIHooks *lifecycle_hooks;
extern SslAPIHooks *ssl_hooks;
extern ConfigUpdateCbTable *global_config_cbs;
