#pragma once

#include <cstddef>
#include <memory>
#include <cassert>

namespace avocado {

    class StdAllocator {
    public:
        template<class T>
        T * allocate(size_t n = 1) const {
            std::allocator<T> alloc;
            std::allocator_traits<std::allocator<T>>::allocate(alloc, n);
        }

        template<class T>
        void deallocate(T * ptr, size_t n) const {
            std::allocator<T> alloc;
            std::allocator_traits<std::allocator<T>>::deallocate(alloc, ptr, n);
        }
    };

    template<class T, class Allocator = StdAllocator>
    class std_allocator {
    public:
        using value_tupe = T;
        using pointer = T *;
        using const_pointer = T const *;
        using reference = T&;
        using const_reference = T const &;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;
        template<typename U>
        struct rebind {
            using other = std_allocator<U, Allocator>;
        };
    
    private:
        Allocator alloc;

    public:
        std_allocator(Allocator && alloc) : alloc(alloc) {}

        T * allocate(size_type n, void const *) {
            return alloc.allocate(n);
        }

        T * allocate(size_type n) {
            return alloc.allocate(n);
        }

        void deallocate(T * ptr, size_type n) {
            return alloc.deallocate(ptr, n);
        }
    };

    template<class T>
    struct List_node {
        T val;
        std::shared_ptr<List_node> next;

        template<class... Args>
        List_node(Args && ... args) : val(std::forward<Args>(args)...), next(nullptr) {}

        List_node(List_node const &) = default;
        List_node(List_node &&) = default;
        List_node(T const & val) : val(val), next(nullptr) {}
        List_node(T && val) : val(std::forward<T>(val)), next(nullptr) {}
    };

    template<class T,
             class _Allocator = std::allocator<T>>
    class List {
    public:
        using Allocator       = typename std::allocator_traits<_Allocator>::template rebind_alloc<List_node<T>>;
        using value_type      = T;
        using allocator_type  = Allocator;
        using size_type       = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = value_type &;
        using const_reference = value_type const &;
        using pointer         = typename std::allocator_traits<Allocator>::pointer;
        using const_pointer   = typename std::allocator_traits<Allocator>::const_pointer;

        using Node = List_node<T>;
        using AllocTraits = std::allocator_traits<Allocator>;

        class ConstIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = T const;
            using difference_type   = std::ptrdiff_t;
            using pointer           = T const *;
            using reference         = value_type const &;

        protected:
            std::shared_ptr<Node> ptr{nullptr};
            bool before_head{false};

        public:
            ConstIterator() = default;
            ConstIterator(std::shared_ptr<Node> const & node, bool before_head = false) : ptr(node), before_head(before_head) {};
            ConstIterator(ConstIterator const & other) : ptr(other.ptr), before_head(other.before_head) {}
            ConstIterator(ConstIterator && other) : ptr(std::move(other.ptr)), before_head(other.before_head) {}
            ~ConstIterator() = default;

            ConstIterator & operator++() noexcept {
                if (before_head) {
                    before_head = false;
                    return *this;
                }
                if (ptr == nullptr)
                    return *this;
                ptr = ptr->next;
                return *this;
            }

            ConstIterator operator++(int) noexcept {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            ConstIterator & operator=(ConstIterator const &) = default;
            bool operator==(ConstIterator const & other) const noexcept { 
              return (before_head == true && other.before_head == true) || (before_head == false && other.before_head == false && ptr == other.ptr);
            }

            bool operator!=(ConstIterator const & other) const noexcept { return !(*this == other);}

            reference operator*() const noexcept {
                //FIXME A FOOOOOOOOO
                return ptr->val;
            }

            pointer operator->() const noexcept {
                if (before_head)
                    return nullptr;
                return &(ptr->val);
            }

            std::shared_ptr<Node> get_ptr() {
                if (before_head)
                  return nullptr;
                return ptr;
            }

        };

        class Iterator : public ConstIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type        = T;
            using ConstIterator::difference_type;
            using pointer           = T *;
            using reference         = T &;

        protected:
            using ConstIterator::ptr;
            using ConstIterator::before_head;

        public:

            Iterator() = default;
            Iterator(std::shared_ptr<Node> & node, bool before_head = false) : ConstIterator(node, before_head) {}
            Iterator(ConstIterator const & iter) : ConstIterator(iter) {}
            
            Iterator & operator++() noexcept {
                if (before_head) {
                    before_head = false;
                    return *this;
                }
                if (ptr == nullptr)
                    return *this;
                ptr = ptr->next;
                return *this;
            }

            Iterator operator++(int) noexcept {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            reference operator*() const noexcept {
                return *ptr;
            }

            pointer operator->() const noexcept {
                return ptr.get();
            }
        };

        using const_iterator  = ConstIterator;
        using iterator = Iterator;

    private:
        Allocator alloc;
        std::shared_ptr<Node> head{nullptr};
        size_type count{0};

        template<class Allocator2>
        class Deleter {
        private:
            Allocator2 alloc;
            using Traits = std::allocator_traits<Allocator2>;

        public:
            Deleter(Allocator2 & alloc) : alloc(alloc) {}

            void operator()(Node * ptr) {
                Traits::destroy(alloc, ptr);
                Traits::deallocate(alloc, ptr, 1);
            }
        };

    public:
        List() = default;
        explicit List(Allocator const & alloc) : alloc(alloc) {};
        
        //TODO Unsupported at the moment
        List(size_type count, T const & value, Allocator const & alloc = Allocator());
        explicit List(size_type count, Allocator const & alloc = Allocator());
        List(List const & other) = default;
        List(List const & other, Allocator const & alloc);
        explicit List(std::initializer_list<T> init, Allocator const & alloc = Allocator());

        ~List() = default;

        List & operator=(List const & other) = delete;
        List & operator=(List && other) noexcept;

        allocator_type get_allocator() const {
            return alloc;
        }

        reference front() {
            return *head;
        }

        const_reference front() const {
            return *head;
        }

        iterator begin() noexcept {
            return iterator(head);
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(head);
        }

        const_iterator begin() const noexcept {
            return cbegin();
        }

        iterator before_begin() noexcept {
            return iterator(head, true);
        }

        const_iterator cbefore_begin() const noexcept {
            return const_iterator(head, true);
        }

        const_iterator before_begin() const noexcept {
            return cbefore_begin();
        }

        iterator end() noexcept {
            return iterator(nullptr);
        }

        const_iterator cend() const noexcept {
            return const_iterator(nullptr);
        }

        const_iterator end() const noexcept {
            return const_iterator(nullptr);
        }

        bool empty() const noexcept {
            return head == nullptr;
        }

        size_type max_size() const noexcept {
            size_type const res{0};
            return ~res;
        }

        void clear() noexcept {
            head.reset();
            count = 0;
        }

        template<class ... Args>
        iterator emplace_after(const_iterator pos, Args && ... args) {
            auto * ptr = AllocTraits::allocate(alloc, 1);
            std::shared_ptr<Node> new_node(ptr, Deleter(alloc));
            AllocTraits::construct(alloc, new_node.get(), std::forward<Args>(args)...);
            if (pos != cbefore_begin()) {
              new_node->next = std::move(pos.get_ptr()->next);
              pos.get_ptr()->next = std::move(new_node);
              return iterator(++pos);
            }
            head = std::move(new_node);
            return begin();
        }

        iterator insert_after(const_iterator pos, T const & value) {
            return emplace_after(pos, value);
        }

        iterator insert_after(const_iterator pos, T && value) {
            return emplace_after(pos, std::forward<T>(value));
        }

        iterator erase_after(const_iterator pos) {
            if (pos != cbefore_begin()) { 
              auto node = pos.get_ptr();
              node->next = std::move(node->next->next);
              return iterator(pos);
            }
            //Stupid special case with before_begin...
            head = std::move(head->next);
            return before_begin();
        }

        template<class ... Args>
        reference emplace_front(Args && ... args) {
            auto * ptr = AllocTraits::allocate(alloc, 1);
            std::shared_ptr<Node> new_node(ptr, Deleter(alloc));
            AllocTraits::construct(alloc, new_node.get(), std::forward<Args>(args)...);
            new_node->next = std::move(head);
            head = std::move(new_node);
            return head->val;
        }

        void push_front(T const & value) {
            emplace_front(value);
        }

        void push_front(T && value) {
            emplace_front(std::forward<T>(value));
        }

    };

} //namespace shieldstore
