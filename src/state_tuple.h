#pragma once
#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
// Owning typed in-memory component snapshots. No raw object copies or pointers.
template<class T> auto state_own(const T& value){static_assert(std::is_integral_v<T>||std::is_enum_v<T>);return value;}
template<class T,std::size_t N> auto state_own(const T (&value)[N]){
    std::array<T,N> out{};for(std::size_t i=0;i<N;++i)out[i]=value[i];return out;
}
template<class T> void state_assign(T& dst,const T& src){static_assert(std::is_integral_v<T>||std::is_enum_v<T>);dst=src;}
template<class T,std::size_t N> void state_assign(T (&dst)[N],const std::array<T,N>& src){for(std::size_t i=0;i<N;++i)dst[i]=src[i];}
template<class Snapshot,class Targets,std::size_t... I>void restore_state_tuple(const Snapshot& state,Targets targets,std::index_sequence<I...>){(state_assign(std::get<I>(targets),std::get<I>(state)),...);}
template<class Snapshot,class... Targets>void restore_state_tuple(const Snapshot& state,std::tuple<Targets&...> targets){
    static_assert(std::tuple_size_v<Snapshot> == sizeof...(Targets));
    restore_state_tuple(state,targets,std::index_sequence_for<Targets...>{});
}
