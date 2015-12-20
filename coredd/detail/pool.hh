#pragma once

#include <memory> // unique_ptr

namespace coredd { namespace detail
{

/*------------------------------------------------------------------------------------------------*/

/// @brief Fixed-size pool allocator for cache entries.
template <typename T>
class pool
{
private:

  union node
  {
    node* next;
    unsigned char data[sizeof(T)];
  };

public:

  pool(std::size_t size)
    : m_head(std::make_unique<node[]>(size))
    , m_free_list(m_head.get())
  {
    for (auto i = 0ul; i < size - 1; ++i)
    {
      m_free_list[i].next = &m_free_list[i+1];
    }
    m_free_list[size - 1].next = nullptr;
  }

  void*
  allocate()
  noexcept
  {
    assert(m_free_list != nullptr);
    void* p = m_free_list;
    m_free_list = m_free_list->next;
    return p;
  }

  void
  deallocate(void* ptr)
  noexcept
  {
    assert(ptr != nullptr);
    node* p = static_cast<node*>(ptr);
    p->next = m_free_list;
    m_free_list = p;
  }

private:

  std::unique_ptr<node[]> m_head;
  node* m_free_list;
};

/*------------------------------------------------------------------------------------------------*/

}} // namespace coredd::detail
