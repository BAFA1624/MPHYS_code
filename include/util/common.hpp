#pragma once

#include "Eigen/Dense"
#include "Eigen/Eigenvalues"
#include "Eigen/Sparse"

#include <complex>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <ranges>
#include <regex>
#include <string>
#include <type_traits>

// Ensure any passed macro is "stringified"
#define STRINGIFY( x ) #x
#define STRING( x )    STRINGIFY( x )

// Defines of correctly typed bitwise operators for a specified enum type
// clang-format off
#define ENUM_FLAG_OP_RAW( enum_t, op )                                     \
    inline enum_t operator op ( const enum_t lhs, const enum_t rhs ) {     \
        return static_cast< enum_t >(                                      \
            static_cast<std::underlying_type_t< enum_t >>( lhs )           \
                op static_cast<std::underlying_type_t< enum_t >>( rhs ) ); \
    }
#define ENUM_FLAG_REF_OP( enum_t, op )                                    \
    inline enum_t & operator op ## = ( enum_t & lhs, const enum_t rhs ) { \
        return static_cast< enum_t & >(                                   \
            static_cast<std::underlying_type_t< enum_t >&>( lhs ) op ## =  \
                static_cast<std::underlying_type_t< enum_t >>( rhs ) );   \
    }
// clang-format on

#define ENUM_FLAG_OP( enum_t, op ) consteval ENUM_FLAG_OP_RAW( enum_t, op );
// ENUM_FLAG_REF_OP( enum_t, op );
// ENUM_FLAG_OP_RAW( enum_t, op );

// clang-format off
#define ENUM_FLAGS( enum_t )                                         \
    static_assert(                                                   \
        std::is_enum< enum_t >::value,                               \
        "Provided type to BIT_OPS must be an enum class. (" __FILE__ \
        ":" STRING( __LINE__ ) ")\n" );                              \
    consteval inline enum_t operator~( const enum_t a ) {            \
        return static_cast< enum_t >(                                \
            ~static_cast<std::underlying_type_t< enum_t >>( a ) );   \
    }                                                                \
    ENUM_FLAG_OP( enum_t, | )                                        \
    ENUM_FLAG_OP( enum_t, & )                                        \
    ENUM_FLAG_OP( enum_t, ^)
// clang-format on

namespace UTIL
{

// Type concepts

// Complex type checker for complex type
template <typename T>
struct is_complex : std::false_type
{};
template <std::floating_point T>
struct is_complex<std::complex<T>> : std::true_type
{};

// Concept for arithmetic types
template <typename T>
concept ArithmeticType = std::is_arithmetic<T>::value;

template <typename T>
concept LessThanComparable =
    std::equality_comparable<T> && requires( const T lhs, const T rhs ) {
        { lhs < rhs } -> std::convertible_to<bool>;
        { lhs > rhs } -> std::convertible_to<bool>;
    };

template <typename T>
concept InStream = std::convertible_to<T, std::istream &>;
template <typename T>
concept OutStream = std::convertible_to<T, std::ostream &>;
template <typename T>
concept Streamable =
    requires( T x, const T y, std::ostream & os, std::istream & is ) {
        { os << y } -> OutStream;
        { is >> x } -> InStream;
    };

// RandomNumberEngine concept
template <typename Engine>
concept RandomNumberEngine =
    requires( Engine e, const typename Engine::result_type seed,
              const unsigned long long n ) {
        requires std::unsigned_integral<typename Engine::result_type>;
        { e.seed( seed ) } -> std::same_as<void>;
        { e.operator()() } -> std::same_as<typename Engine::result_type>;
        { e.discard( n ) } -> std::same_as<void>;
        { e.min() } -> std::same_as<typename Engine::result_type>;
        { e.max() } -> std::same_as<typename Engine::result_type>;
    }
    && std::constructible_from<Engine>
    && std::constructible_from<Engine, const Engine &>
    && std::constructible_from<Engine, typename Engine::result_type>
    && std::equality_comparable<Engine> && Streamable<Engine>;

// TODO: RandomNumberDistribution concept (WIP)

// Concept for weight types
template <typename T>
concept Weight = std::floating_point<T> || is_complex<T>::value;

// Typedef for all integral types. Same as Eigen::Index.
using Index = std::ptrdiff_t;

// Eigen vector typedefs
template <Weight T, Index N = Eigen::Dynamic>
using Vec = Eigen::Vector<T, N>;
template <Weight T>
using SVec = Eigen::SparseVector<T, Eigen::ColMajor, Index>;
template <Weight T, Index N = Eigen::Dynamic>
using RowVec = Eigen::RowVector<T, N>;
template <Weight T>
using RowSVec = Eigen::SparseVector<T, Eigen::RowMajor, Index>;
template <Weight T, Index N = Eigen::Dynamic>
using RefVec = Eigen::Ref<Vec<T, N>>;
template <Weight T>
using RefSVec = Eigen::Ref<SVec<T>>;
template <Weight T, Index N = Eigen::Dynamic>
using RefRowVec = Eigen::Ref<RowVec<T, N>>;
template <Weight T>
using RefRowSVec = Eigen::Ref<RowSVec<T>>;
template <Weight T, Index N = Eigen::Dynamic>
using ConstRefVec = Eigen::Ref<const Vec<T, N>>;
template <Weight T>
using CosntRefSVec = Eigen::Ref<const SVec<T>>;
template <Weight T, Index N = Eigen::Dynamic>
using ConstRefRowVec = Eigen::Ref<const RowVec<T, N>>;
template <Weight T>
using ConstRefRowSVec = Eigen::Ref<const RowSVec<T>>;

// Eigen matrix typedefs
template <Index N = Eigen::Dynamic>
using Indices = Eigen::Vector<Index, N>;
template <Index N = Eigen::Dynamic>
using RefIndices = Eigen::Ref<Indices<N>>;

template <Weight T, Index R = Eigen::Dynamic, Index C = Eigen::Dynamic>
using Mat = Eigen::Matrix<T, R, C>;
template <Weight T, Index R = Eigen::Dynamic, Index C = Eigen::Dynamic>
using RefMat = Eigen::Ref<Mat<T, R, C>>;
template <Weight T, Index R = Eigen::Dynamic, Index C = Eigen::Dynamic>
using ConstRefMat = Eigen::Ref<const Mat<T, R, C>>;
template <Weight T, Eigen::StorageOptions _Options = Eigen::RowMajor,
          std::signed_integral _StorageIndex = Index>
using SMat = Eigen::SparseMatrix<T, _Options, _StorageIndex>;
template <Weight T, Eigen::StorageOptions _Options = Eigen::RowMajor,
          std::signed_integral _StorageIndex = Index>
using RefSMat = Eigen::Ref<SMat<T, _Options, _StorageIndex>>;
template <Weight T, Eigen::StorageOptions _Options = Eigen::RowMajor,
          std::signed_integral _StorageIndex = Index>
using ConstRefSMat = Eigen::Ref<const SMat<T, _Options, _StorageIndex>>;

// Return type for train/test split functions
template <Weight T, Index R = -1, Index C = -1>
using DataPair = std::tuple<Mat<T, R, C>, Mat<T, R, C>>;

// Typedef for representing feature vector data with a column & delay
using FeatureVecShape = std::vector<std::tuple<Index, Index>>;

// String version of Eigen::Matrix shape
template <Weight T, Index R = -1, Index C = -1>
inline std::string
mat_shape_str( const ConstRefMat<T, R, C> & m ) {
    return std::format( "({}, {})", m.rows(), m.cols() );
}
template <Weight T>
inline std::string
mat_shape_str( const ConstRefSMat<T> & m ) {
    return std::format( "({}, {})", m.rows(), m.cols() );
}

template <Weight T>
constexpr inline T
inversion_condition( const ConstRefMat<T> & m ) {
    const auto sing_values = m.jacobiSvd().singularValues();
    return sing_values( Eigen::placeholders::last ) / sing_values( 0 );
}

// Solver concept
template <typename S /* Solver */ /*, typename... Args*/>
concept Solver = requires( S s, const ConstRefMat<typename S::value_t> & X,
                           const ConstRefMat<typename S::value_t> & y ) {
    requires Weight<typename S::value_t>;
    { s.solve( X, y ) } -> std::convertible_to<Mat<typename S::value_t>>;
}; // && std::constructible_from<S, Args...>;

template <Weight T>
class L2Solver
{
    private:
    T m_ridge;

    public:
    L2Solver( const T ridge ) : m_ridge( ridge ) {}

    using value_t = T;

    constexpr inline Mat<T> solve( const ConstRefMat<T> & X,
                                   const ConstRefMat<T> & y ) const noexcept {
        const auto X_2{ X * X.transpose() };
        const auto regularization_matrix{
            m_ridge * Mat<T>::Identity( X_2.rows(), X_2.cols() )
        };
        const auto sum{ X_2 + regularization_matrix };
        const auto factor{ sum.fullPivLu().inverse() };
        return y.transpose() * ( factor * X ).transpose();
    }
};

static_assert( Solver<L2Solver<double>> );

// DataPreprocessor concept
template <typename P>
concept DataProcessor =
    requires( P processor, const ConstRefMat<typename P::value_t> & data ) {
        requires Weight<typename P::value_t>;
        {
            processor.pre_process( data )
        } -> std::convertible_to<Mat<typename P::value_t>>;
    } && std::constructible_from<P>;

template <Weight T>
class NullProcessor
{
    public:
    using value_t = T;
    [[nodiscard]] constexpr inline Mat<T>
    pre_process( const ConstRefMat<T> & data ) const noexcept {
        return data;
    }
};

template <Weight T>
class Normalizer
{
    public:
    using value_t = T;

    [[nodiscard]] constexpr inline Mat<T>
    pre_process( const ConstRefMat<T> & data ) {
        return ( data.colwise() - data.colwise().minCoeff() )
               / ( data.colwise().maxCoeff() - data.colwise().minCoeff() );
    }
};
static_assert( DataProcessor<Normalizer<double>> );

template <Weight T>
class Standardizer
{
    private:
    std::vector<T> m_std;
    std::vector<T> m_mean;

    [[nodiscard]] constexpr inline std::pair<Index, Index>
    dimensions( const ConstRefMat<T> & data ) const noexcept {
        return std::pair{ data.rows(), data.cols() };
    }
    [[nodiscard]] constexpr inline std::vector<T>
    mean( const ConstRefMat<T> & data ) const noexcept {
        std::vector<T> means( static_cast<std::size_t>( data.cols() ) );
        for ( Index i{ 0 }; i < data.cols(); ++i ) {
            means[i] = data.col( i ).mean();
        }
        return means;
    }
    [[nodiscard]] constexpr inline std::vector<T>
    std( const ConstRefMat<T> & data ) const noexcept {
        std::vector<T> std( static_cast<std::size_t>( data.cols() ) );
        for ( Index i{ 0 }; i < data.cols(); ++i ) {
            const auto & mean{ m_mean[i] };
            std[i] = std::sqrt(
                ( data.col( i ) - mean )
                    .unaryExpr( []( const auto x ) { return x * x; } )
                    .sum()
                / static_cast<T>( data.rows() - 1 ) );
        }
        return std;
    }

    public:
    using value_t = T;

    [[nodiscard]] constexpr inline Mat<T>
    pre_process( const ConstRefMat<T> & data ) {
        m_mean = mean( data );
        m_std = std( data );

        Mat<T> standardised( data.rows(), data.cols() );
        for ( Index i{ 0 }; i < data.cols(); ++i ) {
            standardised.col( i ) = ( data.col( i ) - m_mean[i] ) / m_std[i];
        }

        return standardised;
    }
};
static_assert( DataProcessor<Standardizer<double>> );

template <Weight T>
constexpr inline RowVec<T>
RMSE( const ConstRefMat<T> & X, const ConstRefMat<T> & y ) {
    if ( X.rows() != y.rows() || X.cols() != y.cols() ) {
        std::cerr << std::format(
            "RMSE: samples & labels must have matching dimensions (X: "
            "{}, y: {})\n",
            mat_shape_str<T, -1, -1>( X ), mat_shape_str<T, -1, -1>( y ) );
        exit( EXIT_FAILURE );
    }

    return ( ( X - y )
                 .unaryExpr( []( const T x ) { return x * x; } )
                 .colwise()
                 .sum()
             / static_cast<T>( X.rows() ) )
        .unaryExpr( []( const T x ) { return std::sqrt( x ); } );
}

template <Weight T>
constexpr inline Mat<T>
windowed_RMSE( const ConstRefMat<T> & X, const ConstRefMat<T> & y,
               const Index window_length ) {
    if ( X.rows() != y.rows() || X.cols() != y.cols() ) {
        std::cerr << std::format(
            "windowed_RMSE: samples & labels must have matching dimensions (X: "
            "{}, y: {})\n",
            mat_shape_str<T, -1, -1>( X ), mat_shape_str<T, -1, -1>( y ) );
        exit( EXIT_FAILURE );
    }

    if ( window_length > X.rows() || window_length < 0 ) {
        std::cerr << std::format(
            "windowed_RMSE: window_length ({}) must be <= data length ({}) & > "
            "0.\n",
            window_length, X.rows() );

        exit( EXIT_FAILURE );
    }

    const Index n_windows{ X.rows() / window_length },
        remainder{ X.rows() % window_length };

    Mat<T> RMSE_values( n_windows, X.cols() );

    // Window size = window_length
    for ( Index i{ 0 }; i < n_windows - remainder; ++i ) {
        const Index start{ i * window_length },
            end{ ( i + 1 ) * window_length - 1 };

        const ConstRefMat<T> & Xref{ X( Eigen::seq( start, end ),
                                        Eigen::placeholders::all ) };

        const ConstRefMat<T> & yref{ y( Eigen::seq( start, end ),
                                        Eigen::placeholders::all ) };

        RMSE_values( i, Eigen::placeholders::all ) = RMSE( Xref, yref );
    }

    // Window size = window_length + 1
    Index offset{ 0 };
    for ( Index i{ n_windows - remainder }; i < n_windows; ++i ) {
        const Index start{ i * window_length + offset++ },
            end{ start + window_length };

        const ConstRefMat<T> & Xref{ X( Eigen::seq( start, end ),
                                        Eigen::placeholders::all ) };
        const ConstRefMat<T> & yref{ y( Eigen::seq( start, end ),
                                        Eigen::placeholders::all ) };

        RMSE_values( i, Eigen::placeholders::all ) = RMSE( Xref, yref );
    }

    return RMSE_values;
}

inline std::map<std::string, Index>
parse_filename( const std::string_view filename ) {
    const std::string re{
        "([\\d]+)_([01])_([-\\d]+)_([\\d+]+)_([01])_([\\d]+)_([\\d]+)\\.csv"
    };
    const std::regex                                     pattern{ re };
    std::match_results<std::string_view::const_iterator> match;
    std::map<std::string, Index>                         result = {
        { "N", -1 },       { "train_test", -1 },          { "n_points", -1 },
        { "seed", -1 },    { "measured_integrated", -1 }, { "no", -1 },
        { "total_no", -1 }
    };
    if ( std::regex_search( filename.cbegin(), filename.cend(), match,
                            pattern ) ) {
        result["N"] = std::stol( match[0].str() );
        result["train_test"] = std::stol( match[1].str() );
        result["n_points"] = std::stol( match[2].str() );
        result["seed"] = std::stol( match[3].str() );
        result["measured_integrated"] = std::stol( match[4].str() );
        result["no"] = std::stol( match[5].str() );
        result["total_no"] = std::stol( match[6].str() );
    }

    return result;
}

inline std::filesystem::path
get_filename( const std::vector<Index> & params ) {
    std::filesystem::path path{};

    switch ( params[1] ) {
    case 0: {
        path += "train_data";
    } break;
    case 1: {
        path += "test_data";
    } break;
    default: {
        path += "forecast_data";
    } break;
    }

    auto params_joined = params | std::views::transform( []( const Index x ) {
                             return std::to_string( x );
                         } )
                         | std::views::join_with( '_' );

    std::string filename{};
    for ( const auto c : params_joined ) { filename += c; }

    return path /= ( filename + ".csv" );
}

inline std::filesystem::path
get_filename( const std::map<std::string, Index> & file_params ) {
    const auto N{ file_params.at( "N" ) };
    const auto train_test{ file_params.at( "train_test" ) };
    const auto n_points{ file_params.at( "n_points" ) };
    const auto seed{ file_params.at( "seed" ) };
    const auto measured_integrated{ file_params.at( "measured_integrated" ) };
    const auto no{ file_params.at( "no" ) };
    const auto total_no{ file_params.at( "total_no" ) };
    return get_filename( std::vector<Index>{
        N, train_test, n_points, seed, measured_integrated, no, total_no } );
}

// std::filesystem::path
// get_metadata_filename( const std::map<char, Index> & hyperparams ) {}

template <Weight T>
constexpr inline DataPair<T>
train_split( const ConstRefMat<T> & raw_data, const FeatureVecShape & shape,
             const Index warmup_offset, const Index stride = 1 ) {
    Mat<T> data{ raw_data(
        Eigen::seq( Eigen::fix<0>, Eigen::placeholders::last, stride ),
        Eigen::placeholders::all ) };

    const Index max_delay{ std::ranges::max( shape
                                             | std::views::elements<1> ) },
        n{ static_cast<Index>( data.rows() ) },
        d{ static_cast<Index>( shape.size() ) },
        train_size{ n - max_delay - 1 },
        label_size{ n - max_delay - 1 - warmup_offset },
        label_offset{ warmup_offset };

    Mat<T> train_samples( train_size, d ), train_labels( label_size, d );

    for ( const auto [i, feature_data] : shape | std::views::enumerate ) {
        const auto [data_col, delay] = feature_data;
        const auto offset{ max_delay - delay };

        train_samples.col( i ) =
            data( Eigen::seq( offset, Eigen::placeholders::last - delay - 1 ),
                  data_col );

        train_labels.col( i ) =
            data( Eigen::seq( offset + label_offset + 1,
                              Eigen::placeholders::last - delay ),
                  data_col );
    }

    return std::tuple{ train_samples, train_labels };
}

template <Weight T>
DataPair<T>
test_split( const ConstRefMat<T> & raw_data, const FeatureVecShape & shape,
            const Index warmup_offset, const Index stride = 1 ) {
    Mat<T> data{ raw_data(
        Eigen::seq( Eigen::fix<0>, Eigen::placeholders::last, stride ),
        Eigen::placeholders::all ) };

    const Index max_delay{ std::ranges::max( shape
                                             | std::views::elements<0> ) },
        d{ static_cast<Index>( shape.size() ) },
        n{ static_cast<Index>( data.rows() ) },
        test_sz{ n - max_delay - warmup_offset - 1 };

    Mat<T> test_warmup( warmup_offset + 1, d ), test_labels( test_sz, d );

    for ( const auto [i, feature_data] : shape | std::views::enumerate ) {
        const auto [data_col, delay] = feature_data;
        const auto offset{ max_delay - delay };

        test_warmup.col( i ) =
            data( Eigen::seq( offset, offset + warmup_offset ), data_col );


        test_labels.col( i ) =
            data( Eigen::seq( offset + warmup_offset + 1,
                              Eigen::placeholders::last - delay ),
                  data_col );
    }

    return std::tuple{ test_warmup, test_labels };
}

template <Weight T>
constexpr inline std::tuple<DataPair<T>, DataPair<T>>
data_split( const ConstRefMat<T> & train_data, const ConstRefMat<T> & test_data,
            const FeatureVecShape & shape, const Index warmup_offset,
            const Index stride = 1 ) {
    return { train_split<T>( train_data, shape, warmup_offset, stride ),
             test_split<T>( test_data, shape, warmup_offset, stride ) };
}

template <Weight T>
constexpr inline std::tuple<DataPair<T>, DataPair<T>>
data_split( const ConstRefMat<T> & data, const double train_test_ratio,
            const FeatureVecShape & shape, const Index warmup_offset,
            const Index stride = 1 ) {
    const Index train_size{ static_cast<Index>(
        static_cast<double>( data.rows() ) * train_test_ratio ) },
        test_size{ data.rows() - train_size };

    return data_split<T>( data.topRows( train_size ),
                          data.bottomRows( test_size ), shape, warmup_offset,
                          stride );
}

} // namespace UTIL
