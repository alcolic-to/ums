/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef UMS_INTRUSIVE_LIST_H
#define UMS_INTRUSIVE_LIST_H

#include <bit>

#include "types.h"

// NOLINTBEGIN(readability-implicit-bool-conversion)

/**
 * Intrusive list node.
 * It hold pointers to next and previous elements in a list.
 */
struct INode {
    INode* m_prev = nullptr;
    INode* m_next = nullptr;
};

/**
 * Intrusive list.
 * Data structure that holds pointers to the first and the last elements of the list.
 * Class T must have a data member INode which we will use as our list links.
 * User must provide offset to the INode when creating list.
 * For example:
 *
 * ```
 * class TestData {
 * public:
 *   int a = 0;
 *   INode m_node; // node link.
 * };
 *
 * IList<TestData, offsetof(TestData, m_node)> list;
 * ```
 *
 * It is user's responsibility to manage memory for the elements.
 */
template<class T, size_t inode_offset>
class IList {
    static_assert(inode_offset <= sizeof(T) - sizeof(INode));

public:
    IList() noexcept = default;

    static T* data_from_node(INode* node) noexcept
    {
        return std::bit_cast<T*>(std::bit_cast<u8*>(node) - inode_offset);
    }

    static const T* data_from_node(const INode* node) noexcept
    {
        return std::bit_cast<const T*>(std::bit_cast<const u8*>(node) - inode_offset);
    }

    static INode* node_from_data(T* data) noexcept
    {
        return std::bit_cast<INode*>(std::bit_cast<u8*>(data) + inode_offset);
    }

    static const INode* node_from_data(const T* data) noexcept
    {
        return std::bit_cast<const INode*>(std::bit_cast<const u8*>(data) + inode_offset);
    }

    void push_back(T* element) noexcept
    {
        if (!element)
            return;

        INode* node = node_from_data(element);
        node->m_next = nullptr;
        node->m_prev = m_tail;

        if (m_tail)
            m_tail->m_next = node;
        else
            m_head = node;

        m_tail = node;
        ++m_size;
    }

    void push_front(T* element) noexcept
    {
        if (!element)
            return;

        INode* node = node_from_data(element);
        node->m_prev = nullptr;
        node->m_next = m_head;

        if (m_head)
            m_head->m_prev = node;
        else
            m_tail = node;

        m_head = node;
        ++m_size;
    }

    T* pop_front() noexcept { return m_head == nullptr ? nullptr : data_from_node(remove(m_head)); }

    T* pop_back() noexcept { return m_tail == nullptr ? nullptr : data_from_node(remove(m_tail)); }

    T* remove(T* element) noexcept
    {
        if (!element)
            return nullptr;

        return remove(node_from_data(element));
    }

    INode* remove(INode* node) noexcept
    {
        if (!node)
            return nullptr;

        if (node->m_prev)
            node->m_prev->m_next = node->m_next;
        else
            m_head = node->m_next;

        if (node->m_next)
            node->m_next->m_prev = node->m_prev;
        else
            m_tail = node->m_prev;

        node->m_next = nullptr;
        node->m_prev = nullptr;
        --m_size;

        return node;
    }

    T* front() noexcept { return m_head ? data_from_node(m_head) : nullptr; }

    const T* front() const noexcept { return m_head ? data_from_node(m_head) : nullptr; }

    T* back() noexcept { return m_tail ? data_from_node(m_tail) : nullptr; }

    const T* back() const noexcept { return m_tail ? data_from_node(m_tail) : nullptr; }

    [[nodiscard]] bool empty() const noexcept { return m_head == nullptr; }

    [[nodiscard]] sz size() const noexcept { return m_size; }

    class iterator {
    public:
        explicit iterator(INode* node) : m_node(node) {}

        T& operator*() { return *data_from_node(m_node); }

        T* operator->() { return data_from_node()(m_node); }

        iterator& operator++()
        {
            m_node = m_node->m_next;
            return *this;
        }

        bool operator!=(const iterator& other) const { return m_node != other.m_node; }

    private:
        INode* m_node;
    };

    iterator begin() { return iterator(m_head); }

    iterator end() { return iterator(nullptr); }

private:
    INode* m_head = nullptr;
    INode* m_tail = nullptr;
    sz m_size = 0;
};

// NOLINTEND(readability-implicit-bool-conversion)

#endif // UMS_INTRUSIVE_LIST_H
