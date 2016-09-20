// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserializaition (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"

template <class T>
struct LinkedListItem : public T {
    LinkedListItem<T>* next;
};

class PointerWrap;

class PointerWrapSection {
public:
    PointerWrapSection(PointerWrap& p, int ver, const char* title)
        : p_(p), ver_(ver), title_(title) {}
    ~PointerWrapSection();

    bool operator==(const int& v) const {
        return ver_ == v;
    }
    bool operator!=(const int& v) const {
        return ver_ != v;
    }
    bool operator<=(const int& v) const {
        return ver_ <= v;
    }
    bool operator>=(const int& v) const {
        return ver_ >= v;
    }
    bool operator<(const int& v) const {
        return ver_ < v;
    }
    bool operator>(const int& v) const {
        return ver_ > v;
    }

    operator bool() const {
        return ver_ > 0;
    }

private:
    PointerWrap& p_;
    int ver_;
    const char* title_;
};

// Wrapper class
class PointerWrap {
// This makes it a compile error if you forget to define DoState() on non-POD.
// Which also can be a problem, for example struct tm is non-POD on linux, for whatever reason...
#ifdef _MSC_VER
    template <typename T, bool isPOD = std::is_pod<T>::value,
              bool isPointer = std::is_pointer<T>::value>
#else
    template <typename T, bool isPOD = __is_pod(T), bool isPointer = std::is_pointer<T>::value>
#endif
    struct DoHelper {
        static void DoArray(PointerWrap* p, T* x, int count) {
            for (int i = 0; i < count; ++i)
                p->Do(x[i]);
        }

        static void Do(PointerWrap* p, T& x) {
            p->DoClass(x);
        }
    };

    template <typename T>
    struct DoHelper<T, true, false> {
        static void DoArray(PointerWrap* p, T* x, int count) {
            p->DoVoid((void*)x, sizeof(T) * count);
        }

        static void Do(PointerWrap* p, T& x) {
            p->DoVoid((void*)&x, sizeof(x));
        }
    };

public:
    enum Mode {
        MODE_READ = 1, // load
        MODE_WRITE,    // save
        MODE_MEASURE,  // calculate size
        MODE_VERIFY,   // compare
    };

    enum Error {
        ERROR_NONE = 0,
        ERROR_WARNING = 1,
        ERROR_FAILURE = 2,
    };

    u8** ptr;
    Mode mode;
    Error error;

public:
    PointerWrap(u8** ptr_, Mode mode_) : ptr(ptr_), mode(mode_), error(ERROR_NONE) {}
    PointerWrap(unsigned char** ptr_, int mode_)
        : ptr((u8**)ptr_), mode((Mode)mode_), error(ERROR_NONE) {}

    PointerWrapSection Section(const char* title, int ver) {
        return Section(title, ver, ver);
    }

    // The returned object can be compared against the version that was loaded.
    // This can be used to support versions as old as minVer.
    // Version = 0 means the section was not found.
    PointerWrapSection Section(const char* title, int minVer, int ver) {
        char marker[16] = {0};
        int foundVersion = ver;

        strncpy(marker, title, sizeof(marker));
        if (!ExpectVoid(marker, sizeof(marker))) {
            // Might be before we added name markers for safety.
            if (foundVersion == 1 && ExpectVoid(&foundVersion, sizeof(foundVersion)))
                DoMarker(title);
            // Wasn't found, but maybe we can still load the state.
            else
                foundVersion = 0;
        } else
            Do(foundVersion);

        if (error == ERROR_FAILURE || foundVersion < minVer || foundVersion > ver) {
            LOG_ERROR(Common, "Savestate failure: wrong version %d found for %s", foundVersion,
                      title);
            SetError(ERROR_FAILURE);
            return PointerWrapSection(*this, -1, title);
        }
        return PointerWrapSection(*this, foundVersion, title);
    }

    void SetMode(Mode mode_) {
        mode = mode_;
    }
    Mode GetMode() const {
        return mode;
    }
    u8** GetPPtr() {
        return ptr;
    }
    void SetError(Error error_) {
        if (error < error_)
            error = error_;
        if (error > ERROR_WARNING)
            mode = PointerWrap::MODE_MEASURE;
    }

    bool ExpectVoid(void* data, int size) {
        switch (mode) {
        case MODE_READ:
            if (memcmp(data, *ptr, size) != 0)
                return false;
            break;
        case MODE_WRITE:
            memcpy(*ptr, data, size);
            break;
        case MODE_MEASURE:
            break; // MODE_MEASURE - don't need to do anything
        case MODE_VERIFY:
            for (int i = 0; i < size; i++) {
                DEBUG_ASSERT_MSG(
                    ((u8*)data)[i] == (*ptr)[i],
                    "Savestate verification failure: %d (0x%X) (at %p) != %d (0x%X) (at %p).\n",
                    ((u8*)data)[i], ((u8*)data)[i], &((u8*)data)[i], (*ptr)[i], (*ptr)[i],
                    &(*ptr)[i]);
            }
            break;
        default:
            break; // throw an error?
        }
        (*ptr) += size;
        return true;
    }

    void DoVoid(void* data, int size) {
        switch (mode) {
        case MODE_READ:
            memcpy(data, *ptr, size);
            break;
        case MODE_WRITE:
            memcpy(*ptr, data, size);
            break;
        case MODE_MEASURE:
            break; // MODE_MEASURE - don't need to do anything
        case MODE_VERIFY:
            for (int i = 0; i < size; i++) {
                DEBUG_ASSERT_MSG(
                    ((u8*)data)[i] == (*ptr)[i],
                    "Savestate verification failure: %d (0x%X) (at %p) != %d (0x%X) (at %p).\n",
                    ((u8*)data)[i], ((u8*)data)[i], &((u8*)data)[i], (*ptr)[i], (*ptr)[i],
                    &(*ptr)[i]);
            }
            break;
        default:
            break; // throw an error?
        }
        (*ptr) += size;
    }

    template <class K, class T>
    void Do(std::map<K, T*>& x) {
        if (mode == MODE_READ) {
            for (auto it = x.begin(), end = x.end(); it != end; ++it) {
                if (it->second != nullptr)
                    delete it->second;
            }
        }
        T* dv = nullptr;
        DoMap(x, dv);
    }

    template <class K, class T>
    void Do(std::map<K, T>& x) {
        T dv = T();
        DoMap(x, dv);
    }

    template <class K, class T>
    void DoMap(std::map<K, T>& x, T& default_val) {
        unsigned int number = (unsigned int)x.size();
        Do(number);
        switch (mode) {
        case MODE_READ: {
            x.clear();
            while (number > 0) {
                K first = K();
                Do(first);
                T second = default_val;
                Do(second);
                x[first] = second;
                --number;
            }
        } break;
        case MODE_WRITE:
        case MODE_MEASURE:
        case MODE_VERIFY: {
            typename std::map<K, T>::iterator itr = x.begin();
            while (number > 0) {
                K first = itr->first;
                Do(first);
                Do(itr->second);
                --number;
                ++itr;
            }
        } break;
        }
    }

    template <class K, class T>
    void Do(std::multimap<K, T*>& x) {
        if (mode == MODE_READ) {
            for (auto it = x.begin(), end = x.end(); it != end; ++it) {
                if (it->second != nullptr)
                    delete it->second;
            }
        }
        T* dv = nullptr;
        DoMultimap(x, dv);
    }

    template <class K, class T>
    void Do(std::multimap<K, T>& x) {
        T dv = T();
        DoMultimap(x, dv);
    }

    template <class K, class T>
    void DoMultimap(std::multimap<K, T>& x, T& default_val) {
        unsigned int number = (unsigned int)x.size();
        Do(number);
        switch (mode) {
        case MODE_READ: {
            x.clear();
            while (number > 0) {
                K first = K();
                Do(first);
                T second = default_val;
                Do(second);
                x.insert(std::make_pair(first, second));
                --number;
            }
        } break;
        case MODE_WRITE:
        case MODE_MEASURE:
        case MODE_VERIFY: {
            typename std::multimap<K, T>::iterator itr = x.begin();
            while (number > 0) {
                Do(itr->first);
                Do(itr->second);
                --number;
                ++itr;
            }
        } break;
        }
    }

    // Store vectors.
    template <class T>
    void Do(std::vector<T*>& x) {
        T* dv = nullptr;
        DoVector(x, dv);
    }

    template <class T>
    void Do(std::vector<T>& x) {
        T dv = T();
        DoVector(x, dv);
    }

    template <class T>
    void DoPOD(std::vector<T>& x) {
        T dv = T();
        DoVectorPOD(x, dv);
    }

    template <class T>
    void Do(std::vector<T>& x, T& default_val) {
        DoVector(x, default_val);
    }

    template <class T>
    void DoVector(std::vector<T>& x, T& default_val) {
        u32 vec_size = (u32)x.size();
        Do(vec_size);
        x.resize(vec_size, default_val);
        if (vec_size > 0)
            DoArray(&x[0], vec_size);
    }

    template <class T>
    void DoVectorPOD(std::vector<T>& x, T& default_val) {
        u32 vec_size = (u32)x.size();
        Do(vec_size);
        x.resize(vec_size, default_val);
        if (vec_size > 0)
            DoArray(&x[0], vec_size);
    }

    // Store deques.
    template <class T>
    void Do(std::deque<T*>& x) {
        T* dv = nullptr;
        DoDeque(x, dv);
    }

    template <class T>
    void Do(std::deque<T>& x) {
        T dv = T();
        DoDeque(x, dv);
    }

    template <class T>
    void DoDeque(std::deque<T>& x, T& default_val) {
        u32 deq_size = (u32)x.size();
        Do(deq_size);
        x.resize(deq_size, default_val);
        u32 i;
        for (i = 0; i < deq_size; i++)
            Do(x[i]);
    }

    // Store STL lists.
    template <class T>
    void Do(std::list<T*>& x) {
        T* dv = nullptr;
        Do(x, dv);
    }

    template <class T>
    void Do(std::list<T>& x) {
        T dv = T();
        DoList(x, dv);
    }

    template <class T>
    void Do(std::list<T>& x, T& default_val) {
        DoList(x, default_val);
    }

    template <class T>
    void DoList(std::list<T>& x, T& default_val) {
        u32 list_size = (u32)x.size();
        Do(list_size);
        x.resize(list_size, default_val);

        typename std::list<T>::iterator itr, end;
        for (itr = x.begin(), end = x.end(); itr != end; ++itr)
            Do(*itr);
    }

    // Store STL sets.
    template <class T>
    void Do(std::set<T*>& x) {
        if (mode == MODE_READ) {
            for (auto it = x.begin(), end = x.end(); it != end; ++it) {
                if (*it != nullptr)
                    delete *it;
            }
        }
        DoSet(x);
    }

    template <class T>
    void Do(std::set<T>& x) {
        DoSet(x);
    }

    template <class T>
    void DoSet(std::set<T>& x) {
        unsigned int number = (unsigned int)x.size();
        Do(number);

        switch (mode) {
        case MODE_READ: {
            x.clear();
            while (number-- > 0) {
                T it = T();
                Do(it);
                x.insert(it);
            }
        } break;
        case MODE_WRITE:
        case MODE_MEASURE:
        case MODE_VERIFY: {
            typename std::set<T>::iterator itr = x.begin();
            while (number-- > 0)
                Do(*itr++);
        } break;

        default:
            LOG_ERROR(Common, "Savestate error: invalid mode %d.", mode);
        }
    }

    // Store strings.
    void Do(std::string& x) {
        int stringLen = (int)x.length() + 1;
        Do(stringLen);

        switch (mode) {
        case MODE_READ:
            x = (char*)*ptr;
            break;
        case MODE_WRITE:
            memcpy(*ptr, x.c_str(), stringLen);
            break;
        case MODE_MEASURE:
            break;
        case MODE_VERIFY:
            DEBUG_ASSERT_MSG((x == (char*)*ptr),
                             "Savestate verification failure: \"%s\" != \"%s\" (at %p).\n",
                             x.c_str(), (char*)*ptr, ptr);
            break;
        }
        (*ptr) += stringLen;
    }

    void Do(std::wstring& x) {
        int stringLen = sizeof(wchar_t) * ((int)x.length() + 1);
        Do(stringLen);

        switch (mode) {
        case MODE_READ:
            x = (wchar_t*)*ptr;
            break;
        case MODE_WRITE:
            memcpy(*ptr, x.c_str(), stringLen);
            break;
        case MODE_MEASURE:
            break;
        case MODE_VERIFY:
            DEBUG_ASSERT_MSG((x == (wchar_t*)*ptr),
                             "Savestate verification failure: \"%ls\" != \"%ls\" (at %p).\n",
                             x.c_str(), (wchar_t*)*ptr, ptr);
            break;
        }
        (*ptr) += stringLen;
    }

    template <class T>
    void DoClass(T& x) {
        x.DoState(*this);
    }

    template <class T>
    void DoClass(T*& x) {
        if (mode == MODE_READ) {
            if (x != nullptr)
                delete x;
            x = new T();
        }
        x->DoState(*this);
    }

    template <class T>
    void DoArray(T* x, int count) {
        DoHelper<T>::DoArray(this, x, count);
    }

    template <class T>
    void Do(T& x) {
        DoHelper<T>::Do(this, x);
    }

    template <class T>
    void DoPOD(T& x) {
        DoHelper<T>::Do(this, x);
    }

    template <class T>
    void DoPointer(T*& x, T* const base) {
        // pointers can be more than 2^31 apart, but you're using this function wrong if you need
        // that much range
        s32 offset = x - base;
        Do(offset);
        if (mode == MODE_READ)
            x = base + offset;
    }

    template <class T, LinkedListItem<T>* (*TNew)(), void (*TFree)(LinkedListItem<T>*),
              void (*TDo)(PointerWrap&, T*)>
    void DoLinkedList(LinkedListItem<T>*& list_start, LinkedListItem<T>** list_end = nullptr) {
        LinkedListItem<T>* list_cur = list_start;
        LinkedListItem<T>* prev = nullptr;

        while (true) {
            u8 shouldExist = (list_cur ? 1 : 0);
            Do(shouldExist);
            if (shouldExist == 1) {
                LinkedListItem<T>* cur = list_cur ? list_cur : TNew();
                TDo(*this, (T*)cur);
                if (!list_cur) {
                    if (mode == MODE_READ) {
                        cur->next = nullptr;
                        list_cur = cur;
                        if (prev)
                            prev->next = cur;
                        else
                            list_start = cur;
                    } else {
                        TFree(cur);
                        continue;
                    }
                }
            } else {
                if (mode == MODE_READ) {
                    if (prev)
                        prev->next = nullptr;
                    if (list_end)
                        *list_end = prev;
                    if (list_cur) {
                        if (list_start == list_cur)
                            list_start = nullptr;
                        do {
                            LinkedListItem<T>* next = list_cur->next;
                            TFree(list_cur);
                            list_cur = next;
                        } while (list_cur);
                    }
                }
                break;
            }
            prev = list_cur;
            list_cur = list_cur->next;
        }
    }

    void DoMarker(const char* prevName, u32 arbitraryNumber = 0x42) {
        u32 cookie = arbitraryNumber;
        Do(cookie);
        if (mode == PointerWrap::MODE_READ && cookie != arbitraryNumber) {
            LOG_ERROR(Common, "After \"%s\", found %d (0x%X) instead of save marker %d (0x%X). "
                              "Aborting savestate load...",
                      prevName, cookie, cookie, arbitraryNumber, arbitraryNumber);
            SetError(ERROR_FAILURE);
        }
    }
};

inline PointerWrapSection::~PointerWrapSection() {
    if (ver_ > 0) {
        p_.DoMarker(title_);
    }
}
