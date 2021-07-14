#pragma once

template<typename T>
class enumerate {
public:

    struct Item {
        size_t idx;
        typename T::value_type & item;
    };
    
    using value_type = Item;

    class Iterator {
        typename T::iterator it;
        size_t counter;

    public:
        Iterator(typename T::iterator it, size_t counter = 0) : it(it), counter(counter) {}

        Iterator operator++() {
            return Iterator(++it, ++counter);
        }

        bool operator!=(Iterator const & other) {
            return it != other.it;
        }

        typename T::iterator::value_type item() {
            return *it;
        }

        value_type operator*() {
            return {counter, *it};
        }

        size_t idx() {
            return counter;
        }
    };

private:
    T & container;

public:

    enumerate(T & t) : container(t) {}

    Iterator begin() {
        return Iterator(container.begin());
    }

    Iterator end() {
        return Iterator(container.end());
    }
};
