/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GCE_PTHREAD
#define GCE_PTHREAD

// Concurreny classess for Cloud Android projects.
//
// These more or less mimic the interface of the C++ classes:
//   Mutex is similar to std::mutex
//   ConditionVariable is similar to std::condition_variable
//   LockGuard is similar to std::lock_guard
//
// There are some extensions:
//   ScopedThread creates a Thread and joins it when the class is destroyed
//   This comes in handy during unit tests. It should be used cautiously, if
//   at all, in production code because thread creation isn't free.

#include <stdint.h>
#include <pthread.h>
#include <api_level_fixes.h>

#include <MonotonicTime.h>

#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);  \
  void operator=(const TypeName&);
#endif

namespace avd {

class Mutex {
 friend class ConditionVariable;

 public:
  Mutex() {
    pthread_mutex_init(&mutex_, NULL);
  }

  ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }

  void Lock() {
    pthread_mutex_lock(&mutex_);
  }

  void Unlock() {
    pthread_mutex_unlock(&mutex_);
  }

  // TODO(ghartman): Add TryLock if and only if there's a good use case.

 protected:

  pthread_mutex_t* GetMutex() {
    return &mutex_;
  }

  pthread_mutex_t mutex_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class ConditionVariable {
 public:
  explicit ConditionVariable(Mutex* mutex) : mutex_(mutex) {
#if GCE_PLATFORM_SDK_BEFORE(L)
    pthread_cond_init(&cond_, NULL);
#else
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&cond_, &attr);
    pthread_condattr_destroy(&attr);
#endif
  }

  ~ConditionVariable() {
    pthread_cond_destroy(&cond_);
  }

  int NotifyOne() {
    return pthread_cond_signal(&cond_);
  }

  int NotifyAll() {
    return pthread_cond_broadcast(&cond_);
  }

  int Wait() {
    return pthread_cond_wait(&cond_, mutex_->GetMutex());
  }

  int WaitUntil(const avd::time::MonotonicTimePoint& tp) {
    struct timespec ts;
    tp.ToTimespec(&ts);
#if GCE_PLATFORM_SDK_BEFORE(L)
    return pthread_cond_timedwait_monotonic_np(&cond_, mutex_->GetMutex(), &ts);
#else
    return pthread_cond_timedwait(&cond_, mutex_->GetMutex(), &ts);
#endif
  }

 protected:
  Mutex* mutex_;
  pthread_cond_t cond_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

template <typename M> class LockGuard {
 public:
  explicit LockGuard(M& mutex) : mutex_(mutex) {
    mutex_.Lock();
  }

  ~LockGuard() {
    mutex_.Unlock();
  }

 private:
  M& mutex_;
  DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

// Use only in cases where the mutex can't be upgraded to a Mutex.
template<> class LockGuard<pthread_mutex_t> {
 public:
  explicit LockGuard(pthread_mutex_t& mutex) : mutex_(mutex), unlock_(false) {
    unlock_ = (pthread_mutex_lock(&mutex_) == 0);
  }

  ~LockGuard() {
    if (unlock_) {
      pthread_mutex_unlock(&mutex_);
    }
  }

 private:
  pthread_mutex_t& mutex_;
  bool unlock_;
  DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

class ScopedThread {
 public:
  ScopedThread(void* (*start)(void*), void* arg) {
    pthread_create(&thread_, NULL, start, arg);
  }

  ~ScopedThread() {
    void* value;
    pthread_join(thread_, &value);
  }

 protected:
  pthread_t thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedThread);
};

}  // namespace avd

#endif
