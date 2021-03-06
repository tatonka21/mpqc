//
// iterators.h
//
// Contains generalized iterators, ranges, and other utilities for iteration
//
// Copyright (C) 2014 David Hollman
//
// Author: David Hollman
// Maintainer: DSH
// Created: Feb 5, 2014
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef _util_misc_iterators_h
#define _util_misc_iterators_h

// Standard library includes
#include <type_traits>

// Boost includes
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/type_traits.hpp>
#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/range.hpp>
#include <boost/iterator/zip_iterator.hpp>


namespace sc {

namespace {
  template <typename T>
  struct iterable_iterator {
      typedef decltype(typename std::remove_reference<T>::type().begin()) type;
  };

  template <typename T>
  struct iterator_dereference {
      typedef typename std::remove_reference<T>::type::reference type;
  };

  template <typename T>
  struct iterable_iterator_dereference {
      typedef typename iterator_dereference<
        typename iterable_iterator<T>::type
      >::type type;
  };

  template<>
  struct iterator_dereference<boost::tuples::null_type> {
      typedef boost::tuples::null_type type;
  };

  template <typename H, typename T>
  struct iterator_dereference<boost::tuples::cons<H,T>> {
      typedef boost::tuples::cons<
          typename iterator_dereference<H>::type,
          typename iterator_dereference<T>::type
      > type;
  };

  template <typename H, typename T>
  typename boost::enable_if<
    boost::is_same<T, boost::tuples::null_type>,
    boost::tuples::cons<
      typename iterator_dereference<H>::type,
      typename iterator_dereference<T>::type
    >
  >::type dereferencer(const boost::tuples::cons<H, T>& cons_iters);

  template <typename H, typename T>
  typename boost::disable_if<
    boost::is_same<T, boost::tuples::null_type>,
    boost::tuples::cons<
      typename iterator_dereference<H>::type,
      typename iterator_dereference<T>::type
    >
  >::type
  dereferencer(const boost::tuples::cons<H, T>& cons_iters) {
    return boost::tuples::cons<
      typename iterator_dereference<H>::type,
      typename iterator_dereference<T>::type
    >(
        *cons_iters.head,
        dereferencer(cons_iters.tail)
    );
  }

  template <typename H, typename T>
  typename boost::enable_if<
    boost::is_same<T, boost::tuples::null_type>,
    boost::tuples::cons<
      typename iterator_dereference<H>::type,
      typename iterator_dereference<T>::type
    >
  >::type
  dereferencer(const boost::tuples::cons<H, T>& cons_iters) {
    return boost::tuples::cons<
      typename iterator_dereference<H>::type,
      typename iterator_dereference<T>::type
    >(*cons_iters.head, cons_iters.get_tail());
  }

  template <typename Iterable>
  typename iterable_iterator<Iterable>::type
  get_begin(Iterable& container)
  {
    return container.begin();
  }
}

namespace mpl = boost::mpl;

template<typename... Iterables>
class product_iterator
  : public boost::iterator_facade<
      product_iterator<Iterables...>,
      typename boost::tuple<typename iterable_iterator<Iterables>::type...>,   // Value type
      boost::forward_traversal_tag, // CategoryOrTraversal
      typename boost::tuple<typename iterable_iterator_dereference<Iterables>::type...>  // Reference
    >

{
  public:
    typedef boost::tuple<typename iterable_iterator_dereference<Iterables>::type...> reference;
    typedef boost::tuple<typename iterable_iterator<Iterables>::type...> iterators_type;
    typedef boost::tuple<Iterables...> iterables_type;
    typedef product_iterator<Iterables...> self_type;

  private:

    iterators_type spots_;
    iterables_type iterables_;


    friend class boost::iterator_core_access;

    reference dereference() const {
      return static_cast<reference>(
          dereferencer(spots_)
      );
    }


    template<typename... OtherIterTypes>
    bool equal(product_iterator<OtherIterTypes...> const& other) const
    {
      return spots_ == other.spots_;
    }


    template<typename H, typename T, typename H2, typename T2>
    typename boost::disable_if_c<
        boost::is_same<T, boost::tuples::null_type>::value,
        bool
    >::type
    increment_impl(
        boost::tuples::cons<H, T>& cons_iters,
        boost::tuples::cons<H2, T2>& cons_containers
    )
    {
      if(increment_impl(cons_iters.tail, cons_containers.tail)) {
        ++(cons_iters.head);
        if(cons_iters.head == cons_containers.head.end()) {
          cons_iters.head = cons_containers.head.begin();
          return true;
        }
      }
      return false;
    }

    template<typename H, typename T, typename H2, typename T2>
    typename boost::enable_if_c<
        boost::is_same<T, boost::tuples::null_type>::value,
        bool
    >::type
    increment_impl(
        boost::tuples::cons<H, T>& cons_iters,
        boost::tuples::cons<H2, T2>& cons_containers
    )
    {
      ++(cons_iters.head);
      if(cons_iters.head == cons_containers.head.end()) {
        cons_iters.head = cons_containers.head.begin();
        return true;
      }
      return false;
    }

    // Outermost iterator needs to be treated specially
    void increment_impl(
        iterators_type& cons_iters,
        iterables_type& cons_containers
    )
    {
      if(increment_impl(cons_iters.tail, cons_containers.tail)) {
        ++(cons_iters.head);
      }
    }

    void increment()
    {
      increment_impl(spots_, iterables_);
    }

  public:

    explicit product_iterator(Iterables&&... iters)
      : iterables_(iters...),
        spots_(get_begin(iters)...)
    { }

    static self_type end_iterator(Iterables&&... iters)
    {
      auto rv = self_type(std::forward<Iterables>(iters)...);
      boost::tuples::get<0>(rv.spots_) = boost::tuples::get<0>(rv.iterables_).end();
      return rv;
    }

};

template<typename... Iterables>
boost::iterator_range<product_iterator<Iterables...>>
product_range(Iterables&&... iterables)
{
  return boost::make_iterator_range(
      product_iterator<Iterables...>(
          std::forward<Iterables>(iterables)...
      ),
      product_iterator<Iterables...>::end_iterator(
          std::forward<Iterables>(iterables)...
      )
  );
}

////////////////////////////////////////////////////////////////////////////////

/*
Messy and doesn't work!

template<typename Iterable1, typename Iterable2,
  typename function_argument = typename iterable_iterator_dereference<Iterable1>::type
>
class dependent_pair_product_iterator
  : public boost::iterator_facade<
      dependent_pair_product_iterator<Iterable1, Iterable2>,
      typename boost::tuple<
        typename iterable_iterator<Iterable1>::type,
        typename iterable_iterator<Iterable2>::type
      >, // Value type
      boost::forward_traversal_tag, // CategoryOrTraversal
      boost::tuple<
        typename iterable_iterator_dereference<Iterable1>::type,
        typename iterable_iterator_dereference<Iterable2>::type
      >  // Reference
    >
{

  public:

    typedef typename iterable_iterator_dereference<Iterable1>::type reference1;
    typedef typename iterable_iterator_dereference<Iterable2>::type reference2;
    typedef boost::tuple<reference1, reference2> reference;
    typedef typename iterable_iterator<Iterable1>::type Iterator1;
    typedef typename iterable_iterator<Iterable2>::type Iterator2;
    typedef dependent_pair_product_iterator<Iterable1, Iterable2> self_type;
    typedef std::function<Iterable2(function_argument)> iterable2_getter_type;

  protected:

    Iterable1 iterable1_;
    Iterable2 curr_iterable2_;
    Iterator1 spot1_;
    Iterator2 spot2_;
    const iterable2_getter_type& iterable2_getter_;

    friend class boost::iterator_core_access;

    reference dereference() const {
      return boost::make_tuple(*spot1_, *spot2_);
    }

    void increment() {
      ++spot2_;
      if(spot2_ == curr_iterable2_.end()) {
        ++spot1_;
        if(spot1_ != iterable1_.end()) {
          curr_iterable2_ = iterable2_getter_(*spot1_);
          spot2_ = curr_iterable2_.begin();
        }
      }
    }

    template<typename Iter1, typename Iter2>
    bool equal(dependent_pair_product_iterator<Iter1, Iter2> const& other) const
    {
      return spot1_ == other.spot1_ and spot2_ == other.spot2_;
    }

  public:

    dependent_pair_product_iterator(
        Iterable1&& iterable1,
        iterable2_getter_type&& iterable2_getter
    ) : iterable1_(iterable1),
        iterable2_getter_(iterable2_getter),
        spot1_(iterable1_.begin()),
        curr_iterable2_(iterable2_getter_(*spot1_)),
        spot2_(curr_iterable2_.begin())
    { }

    static self_type end_iterator(
        Iterable1&& iterable1,
        iterable2_getter_type&& iterable2_getter
    )
    {
      self_type rv(iterable1, iterable2_getter);
      rv.spot1_ = rv.iterable1_.end();
      rv.spot1_--;
      rv.curr_iterable2_ = rv.iterable2_getter_(*rv.spot1_);
      rv.spot2_ = rv.curr_iterable2_.end();
      rv.spot1_++;
      return rv;
    }

};

template<typename Iterable1, typename Iterable2Getter>
boost::iterator_range<
  dependent_pair_product_iterator<
    Iterable1,
    typename Iterable2Getter::result_type,
    typename Iterable2Getter::argument_type
  >
>
dependent_product_range(
    Iterable1&& i1,
    Iterable2Getter&& i2_getter
)
{
  typedef typename Iterable2Getter::result_type Iterable2;
  typedef typename Iterable2Getter::argument_type function_argument_type;
  return boost::make_iterator_range(
      dependent_pair_product_iterator<
        Iterable1, Iterable2, function_argument_type
      >(
          std::forward<Iterable1>(i1),
          std::forward<Iterable2Getter>(i2_getter)
      ),
      dependent_pair_product_iterator<
        Iterable1, Iterable2, function_argument_type
      >::end_iterator(
          std::forward<Iterable1>(i1),
          std::forward<Iterable2Getter>(i2_getter)
      )
  );
}

*/

////////////////////////////////////////////////////////////////////////////////

namespace {

  // can_advance = true case
  template<bool can_advance, typename Iterator, typename difference_type>
  struct advance_iterator {
    void operator()(
        Iterator& it,
        const difference_type& n_adv,
        const Iterator& end_iter
    ) const
    {
      it += std::min(
          end_iter - it,
          (typename std::iterator_traits<Iterator>::difference_type) n_adv
      );
    }
  };

  template<typename Iterator, typename difference_type>
  struct advance_iterator<false, Iterator, difference_type> {
    void operator()(
        Iterator& it,
        const difference_type& n_adv,
        const Iterator& end_iter
    ) const
    {
      // assume only that Iterator is incrementable
      for(int i = 0; i < n_adv; ++i, ++it) {
        if(it == end_iter) {
          break;
        }
      }
    }
  };

} // end of anonymous namespace

template<typename Iterator, bool base_can_be_advanced=false>
class threaded_iterator
  : public boost::iterator_adaptor<
      threaded_iterator<Iterator>,
      Iterator,
      boost::use_default,
      boost::forward_traversal_tag
    >
{
  public:

    typedef typename threaded_iterator::iterator_adaptor_ super_t;

  private:

    friend class boost::iterator_core_access;

    Iterator end_iter_;
    int ithr_;
    int nthr_;

    void increment() {
      advance_iterator<
        base_can_be_advanced,
        Iterator,
        decltype(nthr_)
      >()(
          super_t::base_reference(), nthr_, end_iter_
      );
    }

  public:

    threaded_iterator(Iterator&& iter, Iterator&& end_iter, int ithr, int nthr)
      : super_t(iter), end_iter_(end_iter), ithr_(ithr), nthr_(nthr)
    {
      advance_iterator<
        base_can_be_advanced,
        Iterator,
        decltype(ithr_)
      >()(
          super_t::base_reference(), ithr_, end_iter_
      );
    }

};



template<typename Iterator>
using skip_iterator = threaded_iterator<Iterator>;

template <
  typename Range,
  bool base_can_be_advanced=false
>
boost::iterator_range<threaded_iterator<typename iterable_iterator<Range>::type>>
thread_over_range(Range&& range, int ithr, int nthr)
{
  return boost::make_iterator_range(
      threaded_iterator<
        decltype(range.begin()), base_can_be_advanced
      >(range.begin(), range.end(), ithr, nthr),
      threaded_iterator<
        decltype(range.begin()), base_can_be_advanced
      >(range.end(), range.end(), ithr, nthr)
  );
}

template <
  typename Range,
  bool base_can_be_advanced=false
>
boost::iterator_range<threaded_iterator<typename iterable_iterator<Range>::type>>
thread_node_parallel_range(Range&& range, int n_node, int me, int nthread, int ithr)
{
  const int thr_offset = me*nthread + ithr;
  const int increment = n_node*nthread;
  return boost::make_iterator_range(
      threaded_iterator<
        decltype(range.begin()), base_can_be_advanced
      >(range.begin(), range.end(), thr_offset, increment),
      threaded_iterator<
        decltype(range.begin()), base_can_be_advanced
      >(range.end(), range.end(), thr_offset, increment)
  );
}

////////////////////////////////////////////////////////////////////////////////

// From http://stackoverflow.com/questions/8511035/sequence-zip-function-for-c11
// Make sure the containers are the same length.  Behaviour is undefined otherwise
// TODO Length checking, if possible
template <typename... T>
auto zip(const T&... containers)
  -> boost::iterator_range<boost::zip_iterator<decltype(boost::make_tuple(std::begin(containers)...))>>
{
    auto zip_begin = boost::make_zip_iterator(boost::make_tuple(std::begin(containers)...));
    auto zip_end = boost::make_zip_iterator(boost::make_tuple(std::end(containers)...));
    return boost::make_iterator_range(zip_begin, zip_end);
}



} // end namespace sc

#endif /* _util_misc_iterators_h */
