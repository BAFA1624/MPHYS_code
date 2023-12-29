#include "CSV/simple_csv.hpp"
#include "ESN/ESN.hpp"
#include "NVAR/NVAR.hpp"
#include "nlohmann/json.hpp"

#include <array> //exec
#include <chrono>
#include <cstdio> // exec
#include <format>
#include <iostream>
#include <memory> // exec
// #include <stdexcept> // exec
#include <string> // exec
#include <tuple>

using namespace UTIL;

template <typename T>
concept Printable = requires( const T x ) { std::cout << x; };

template <Printable T>
std::ostream &
operator<<( std::ostream & stream, const std::vector<T> & v ) {
    stream << "[ ";
    for ( const auto & x : v | std::views::take( v.size() - 1 ) ) {
        stream << x << " ";
    }
    stream << v.back() << " ]";
    return stream;
}

template <Printable T>
void
print( const std::vector<T> & v ) {
    for ( const auto & x : v ) { std::cout << x << "  "; }
    std::cout << std::endl;
}

std::tuple<int, std::string>
exec( const char * const cmd ) {
    std::string full_cmd{ cmd };
    full_cmd += " 2>&1";

    std::array<char, 128>                      buffer;
    std::unique_ptr<FILE, decltype( &pclose )> pipe(
        popen( full_cmd.c_str(), "r" ), pclose );

    if ( !pipe ) {
        return { 1, std::string{ "popen() failed to open." } };
    }

    std::string std_output;
    while ( fgets( buffer.data(), buffer.size(), pipe.get() ) != nullptr ) {
        std_output += buffer.data();
    }

    return { 0, std_output };
}

template <typename T, Index R = -1, Index C = -1>
std::string
shape_str( const ConstRefMat<T, R, C> m ) {
    return std::format( "({}, {})", m.rows(), m.cols() );
}

template <UTIL::Weight T, RandomNumberEngine Generator>
T
normal_0_2( [[maybe_unused]] const T x, Generator & gen ) {
    static std::normal_distribution<T> dist( 20., 2.0 );
    return dist( gen );
}

int
main( [[maybe_unused]] int argc, [[maybe_unused]] char * argv[] ) {
#ifdef RMSE_TEST
    using T = double;
    const auto x{ UTIL::Mat<T>::Random( 100, 3 ) };
    const auto dx{ UTIL::Mat<T>::Random( 100, 3 ) };
    const auto dy{ UTIL::Mat<T>::Constant( 100, 3, 2. ) };
    const auto x_{ x + dx };
    const auto y{ x + dy };
    const auto RMSE{ UTIL::RMSE<T>( x_, x ) };
    const auto RMSE_{ UTIL::RMSE<T>( x, y ) };

    std::cout << "RMSE:\n" << RMSE << std::endl;
    std::cout << "RMSE_:\n" << RMSE_ << std::endl;

    const auto window_RMSE_10{ UTIL::windowed_RMSE<T>( x, y, 1 ) };
    std::cout << std::endl;
    const auto window_RMSE_11{ UTIL::windowed_RMSE<T>( x, y, 11 ) };

    std::cout << "window_RMSE_10:\n" << window_RMSE_10 << std::endl;
    std::cout << "window_RMSE_11:\n" << window_RMSE_11 << std::endl;
#endif
    const auto n_thread{ Eigen::nbThreads() };
    std::cout << std::format( "Running with {} threads.\n", n_thread );
    Eigen::setNbThreads( 8 );
#ifdef ESN_TEST
    using T = double;

    const std::filesystem::path data_path{
        "../data/train_test_src/17_measured.csv"
    };
    const auto data_csv{ CSV::SimpleCSV(
        /*filename*/ data_path, /*col_titles*/ true, /*skip_header*/ 0,
        /*delim*/ ",", /*max_line_size*/ 256 ) };
    const auto data_pool{ data_csv.atv( 0, 0 ) };

    using T = double;
    const unsigned seed{ 69 };
    const Index    d{ 2 }, n_node{ 1000 }, n_warmup{ 200 }, data_stride{ 3 };
    const T        leak{ 0.95 }, sparsity{ 0.2 }, spectral_radius{ 0.9 },
        alpha{ 1E-6 }, bias{ 1. };

    const auto activation_func = []<UTIL::Weight T>( const T x ) {
        return std::tanh( x );
    };
    const auto solver = UTIL::L2Solver<T>( T{ alpha } );
    const auto w_in_func =
        []<UTIL::Weight T, UTIL::RandomNumberEngine Generator>(
            [[maybe_unused]] const T x, [[maybe_unused]] Generator & gen ) {
            static auto dist{ std::uniform_real_distribution<T>( -1., 1. ) };
            return dist( gen );
        };
    const auto adjacency_func =
        []<UTIL::Weight T, UTIL::RandomNumberEngine Generator>(
            [[maybe_unused]] const T x, [[maybe_unused]] Generator & gen ) {
            static auto dist{ std::uniform_real_distribution<T>( -1., 1. ) };
            return dist( gen );
        };

    std::cout << std::format(
        "d: {}, n_node: {}, leak: {}, sparsity: {}, spectral_radius: {}, seed: "
        "{}, n_warmup: {}\n",
        d, n_node, leak, sparsity, spectral_radius, seed, n_warmup );
    std::cout << std::format( "data_stride: {}\n", data_stride );

    const std::vector<std::tuple<Index, Index>> feature_shape{ { 0, 0 },
                                                               { 1, 0 },
                                                               { 2, 0 } };

    const auto [train_pair, test_pair] =
        data_split<double>( data_pool, 0.75, feature_shape, 0, data_stride,
                            UTIL::Standardizer<T>{} );
    const auto [train_samples, train_labels] = train_pair;
    const auto [test_warmup, test_labels] = test_pair;

    std::cout << std::format( "train_samples: {}, train_labels: {}\n",
                              shape_str<double, -1, -1>( train_samples ),
                              shape_str<double, -1, -1>( train_labels ) );
    std::cout << std::format( "test_warmup: {}, test_labels: {}\n",
                              shape_str<double, -1, -1>( test_warmup ),
                              shape_str<double, -1, -1>( test_labels ) );

    const auto init_start{ std::chrono::steady_clock::now() };
    ESN::ESN</* value_t */ T,
             /* input weight type */ ESN::input_t::sparse
                 | ESN::input_t::homogeneous,
             /* adjacency matrix type */ ESN::adjacency_t::sparse,
             /* feature_shape */ ESN::feature_t::bias | ESN::feature_t::linear
                 | ESN::feature_t::reservoir,
             /* Solver type */ L2Solver<T>,
             /* generator type */ std::ranlux24_base,
             /* target_difference */ true>
        split_dense( d, n_node, leak, sparsity, spectral_radius, seed, n_warmup,
                     bias, activation_func, solver, w_in_func, adjacency_func );

    const auto init_finish{ std::chrono::steady_clock::now() };

    const auto train_start{ std::chrono::steady_clock::now() };
    split_dense.train( train_samples.rightCols( d ),
                       train_labels.rightCols( d ) );

    const auto train_finish{ std::chrono::steady_clock::now() };

    const auto forecast_start{ std::chrono::steady_clock::now() };

    const auto forecast{ split_dense.forecast( test_labels.rightCols( d ),
                                               { 0 } ) };

    const auto forecast_finish{ std::chrono::steady_clock::now() };

    std::cout << std::format(
        "Initialising ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>( init_finish
                                                              - init_start ) );
    std::cout << std::format(
        "Training ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>( train_finish
                                                              - train_start ) );
    std::cout << std::format(
        "Forecasting ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>(
            forecast_finish - forecast_start ) );

    // Write results to file
    // Write location
    const std::filesystem::path write_path{
        "../data/forecast_data/esn_forecast.csv"
    };

    const std::vector<std::string> col_titles{ "t", "I", "V", "I'", "V'" };

    Mat<T> forecast_data( forecast.rows(),
                          static_cast<UTIL::Index>( col_titles.size() ) );

    std::cout << std::format( "test_labels: {}, forecast: {}\n",
                              UTIL::mat_shape_str<T, -1, -1>( test_labels ),
                              UTIL::mat_shape_str<T, -1, -1>( forecast ) );

    forecast_data << test_labels.leftCols( 1 ).bottomRows( test_labels.rows()
                                                           - n_warmup ),
        forecast,
        test_labels.rightCols( d ).bottomRows( test_labels.rows() - n_warmup );

    // Write to a csv
    const auto write_success{ CSV::SimpleCSV::write<T>(
        write_path, forecast_data, col_titles ) };

    if ( !write_success ) {
        std::cerr << std::format( "Unable to write forecast data.\n" );

        return EXIT_FAILURE;
    }
#endif
#ifdef ESN_DOUBLESCROLL
    using T = double;

    // Doublescroll
    const std::filesystem::path doublescroll_train_path{
        "../data/train_data/doublescroll.csv"
    };
    const std::filesystem::path doublescroll_test_path{
        "../data/test_data/doublescroll.csv"
    };
    // clang-format off
    const auto doublescroll_train_csv{ CSV::SimpleCSV(
        /*filename=*/doublescroll_train_path,
        /*col_titles=*/true,
        /*skip_header=*/0,
        /*delim*/ ",",
        /*max_line_size=*/256 ) };
    const auto doublescroll_test_csv{ CSV::SimpleCSV(
        /*filename=*/doublescroll_test_path,
        /*col_titles=*/true,
        /*skip_header=*/0,
        /*delim*/ ",",
        /*max_line_size=*/256 ) };
    // clang-format on

    const auto doublescroll_train_data{ doublescroll_train_csv.atv<double>() };
    const auto doublescroll_test_data{ doublescroll_test_csv.atv<double>() };

    const FeatureVecShape doublescroll_feature_shape{ { 1, 0 },
                                                      { 2, 0 },
                                                      { 3, 0 } };

    const unsigned seed{ 0 };
    const Index    d{ 3 }, n_node{ 400 }, n_warmup{ 0 }, data_stride{ 1 };
    const T        leak{ 0.25 }, sparsity{ 0.1 }, spectral_radius{ 0.3 },
        alpha{ 1.5E-10 }, bias{ 1. };

    const auto activation_func = []<UTIL::Weight T>( const T x ) {
        return std::tanh( x );
    };
    const auto solver = UTIL::L2Solver<T>( T{ alpha } );
    const auto w_in_func =
        []<UTIL::Weight T, UTIL::RandomNumberEngine Generator>(
            [[maybe_unused]] const T x, [[maybe_unused]] Generator & gen ) {
            static auto dist{ std::uniform_real_distribution<T>( -1., 1. ) };
            return dist( gen );
        };
    const auto adjacency_func =
        []<UTIL::Weight T, UTIL::RandomNumberEngine Generator>(
            [[maybe_unused]] const T x, [[maybe_unused]] Generator & gen ) {
            static auto dist{ std::uniform_real_distribution<T>( -1., 1. ) };
            return dist( gen );
        };

    std::cout << std::format(
        "d: {}, n_node: {}, leak: {}, sparsity: {}, spectral_radius: {}, seed: "
        "{}, n_warmup: {}\n",
        d, n_node, leak, sparsity, spectral_radius, seed, n_warmup );
    std::cout << std::format( "data_stride: {}\n", data_stride );

    const auto init_start{ std::chrono::steady_clock::now() };
    ESN::ESN</* value_t */ T,
             /* input weight type */ ESN::input_t::sparse | ESN::input_t::split,
             /* adjacency matrix type */ ESN::adjacency_t::sparse,
             /* feature_shape */ ESN::feature_t::bias | ESN::feature_t::linear
                 | ESN::feature_t::reservoir,
             /* Solver type */ L2Solver<T>,
             /* generator type */ std::ranlux24_base,
             /* target_difference */ true>
        split_dense( d, n_node, leak, sparsity, spectral_radius, seed, n_warmup,
                     bias, activation_func, solver, w_in_func, adjacency_func );

    const auto init_finish{ std::chrono::steady_clock::now() };

    const auto [doublescroll_train_pair, doublescroll_test_pair] =
        data_split<double>( doublescroll_train_data, doublescroll_test_data,
                            doublescroll_feature_shape, 0, data_stride,
                            UTIL::Standardizer<T>{} );
    const auto [doublescroll_train_samples, doublescroll_train_labels] =
        doublescroll_train_pair;
    const auto [doublescroll_warmup, doublescroll_test_labels] =
        doublescroll_test_pair;

    const auto train_start{ std::chrono::steady_clock::now() };
    split_dense.train( doublescroll_train_samples.rightCols( d ),
                       doublescroll_train_labels.rightCols( d ) );

    const auto train_finish{ std::chrono::steady_clock::now() };

    const auto forecast_start{ std::chrono::steady_clock::now() };

    const auto forecast{ split_dense.forecast(
        doublescroll_test_labels.rightCols( d ), { 0, 1 } ) };

    const auto forecast_finish{ std::chrono::steady_clock::now() };

    std::cout << std::format(
        "Initialising ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>( init_finish
                                                              - init_start ) );
    std::cout << std::format(
        "Training ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>( train_finish
                                                              - train_start ) );
    std::cout << std::format(
        "Forecasting ESN took: {}\n",
        std::chrono::duration_cast<std::chrono::duration<T>>(
            forecast_finish - forecast_start ) );

    // Write results to file
    // Write location
    const std::filesystem::path write_path{
        "../data/forecast_data/esn_forecast.csv"
    };

    const std::vector<std::string> col_titles{ "t",  "I",   "V1", "V2",
                                               "I'", "V1'", "V2'" };

    Mat<T> forecast_data( forecast.rows(),
                          static_cast<UTIL::Index>( col_titles.size() ) );

    forecast_data << doublescroll_test_labels.leftCols( 1 ), forecast,
        doublescroll_test_labels.rightCols( d );

    // Write to a csv
    const auto write_success{ CSV::SimpleCSV::write<T>(
        write_path, forecast_data, col_titles ) };

    if ( !write_success ) {
        std::cerr << std::format( "Unable to write forecast data.\n" );

        return EXIT_FAILURE;
    }
#endif
#ifdef FORECAST
    const std::filesystem::path data_path{
        "../data/train_test_src/17_measured.csv"
    };
    const auto data_csv{ CSV::SimpleCSV(
        /*filename*/ data_path, /*col_titles*/ true, /*skip_header*/ 0,
        /*delim*/ ",", /*max_line_size*/ 256 ) };
    const auto data_pool{ data_csv.atv( 0, 0 ) };

    const bool   use_const{ true };
    const double alpha{ 1E-3 }, constant{ 1 };
    const Index  d{ 2 }, k{ 3 }, s{ 2 }, p{ 3 }, data_stride{ 3 };
    std::cout << std::format( "data_stride: {}\n", data_stride );
    std::cout << std::format( "alpha: {}, use_const: {}, constant: {}\n", alpha,
                              use_const ? "true" : "false", constant );
    std::cout << std::format( "d = {}, k = {}, s = {}, p = {}\n", d, k, s, p );

    const std::vector<std::tuple<Index, Index>> feature_shape{ { 0, 0 },
                                                               { 1, 0 },
                                                               { 2, 0 } };

    // const auto [train_samples, train_labels] = train_split<double>(
    //     train_data_pool, feature_shape, k, s, data_stride );
    // const auto [test_warmup, test_labels] = test_split<double>(
    //     test_data_pool, feature_shape, k, s, data_stride );

    const auto [train_pair, test_pair] = data_split<double>(
        data_pool, 0.75, feature_shape, NVAR::warmup_offset( k, s ),
        data_stride, UTIL::Standardizer<double>{} );
    const auto [train_samples, train_labels] = train_pair;
    const auto [test_warmup, test_labels] = test_pair;

    std::cout << std::format( "train_samples: {}, train_labels: {}\n",
                              shape_str<double, -1, -1>( train_samples ),
                              shape_str<double, -1, -1>( train_labels ) );
    std::cout << std::format( "test_warmup: {}, test_labels: {}\n",
                              shape_str<double, -1, -1>( test_warmup ),
                              shape_str<double, -1, -1>( test_labels ) );

    std::cout << "Training NVAR.\n";
    NVAR::NVAR<double, NVAR::nonlinear_t::poly> test(
        train_samples.rightCols( d ), train_labels.rightCols( d ), d, k, s, p,
        use_const, constant, L2Solver<double>( alpha ), true, { "I", "V" },
        "../data/forecast_data/tmp.csv" );


    std::cout << "Forecasting.\n";
    auto forecast{ test.forecast( test_warmup.rightCols( d ),
                                  test_labels.rightCols( d ),
                                  std::vector<Index>{ 0 } ) };

    // Write results out
    std::cout << "Writing results.\n";
    const std::filesystem::path forecast_path{
        "../data/forecast_data/forecast.csv"
    };
    const std::vector<std::string> forecast_col_titles{ "t", "I", "V", "I'",
                                                        "V'" };
    std::cout << std::format(
        "forecast_col_titles.size(): {}, times.cols(): {}, forecast.cols(): "
        "{}, test_labels.cols(): {}\n",
        forecast_col_titles.size(), test_labels.cols(), forecast.cols(),
        test_labels.cols() );
    Mat<double> forecast_data( forecast.rows(),
                               forecast.cols() + test_labels.cols() );

    forecast_data << test_labels.leftCols( 1 ), forecast,
        test_labels.rightCols( d );

    const auto write_success{ CSV::SimpleCSV::write<double>(
        forecast_path, forecast_data, forecast_col_titles ) };

    if ( !write_success ) {
        std::cerr << std::format( "Unable to write forecast data.\n" );
    }

#endif
#ifdef CUSTOM_FEATURES
    const std::filesystem::path data_path{
        "../data/train_test_src/22_measured.csv"
    };
    const auto data_csv{ CSV::SimpleCSV(
        /*filename*/ data_path, /*col_titles*/ true, /*skip_header*/ 0,
        /*delim*/ ",", /*max_line_size*/ 256 ) };
    const auto data_pool{ data_csv.atv( 0, 0 ) };

    const bool   use_const{ true };
    const double alpha{ 0 }, constant{ 1 };
    const Index  d{ 4 }, k{ 2 }, s{ 10 }, p{ 3 }, data_stride{ 4 }, delay{ 1 };
    std::cout << std::format( "data_stride: {}, delay: {}\n", data_stride,
                              delay );
    std::cout << std::format( "alpha: {}, use_const: {}, constant: {}\n", alpha,
                              use_const ? "true" : "false", constant );
    std::cout << std::format( "d = {}, k = {}, s = {}, p = {}\n", d, k, s, p );

    // Feature shape of t_(n), V_(n), V_(n-1), I_(n), I_(n-1)
    const FeatureVecShape feature_shape{
        { 0, 0 }, { 2, 0 }, { 2, delay }, { 1, 0 }, { 1, delay }
    };

    // Get train / test data
    const auto [train_pair, test_pair] = data_split<double>(
        data_pool, 0.75, feature_shape, NVAR::warmup_offset( k, s ),
        data_stride, UTIL::NullProcessor<double>{} );
    const auto [train_samples, train_labels] = train_pair;
    const auto [test_warmup, test_labels] = test_pair;

    // Building NVAR
    std::cout << "Building NVAR.\n";
    NVAR::NVAR<double, NVAR::nonlinear_t::poly> test(
        train_samples.rightCols( d ), train_labels.rightCols( d ), d, k, s, p,
        use_const, double{ constant }, L2Solver( alpha ), true,
        { "V_(n)", "V_(n-1)", "I_(n)", "I_(n-1)" },
        "../data/forecast_data/tmp.csv" );

    std::cout << "Forecasting.\n";
    auto forecast{ test.forecast( test_warmup.rightCols( d ),
                                  test_labels.rightCols( d ),
                                  std::vector<Index>{ 2, 3 } ) };

    // Write results out
    std::cout << "Writing results.\n";
    const std::filesystem::path forecast_path{
        "../data/forecast_data/forecast.csv"
    };
    const std::vector<std::string> forecast_col_titles{
        "t",      "V_(n)",    "V_(n-1)", "I_(n)",   "I_(n-1)",
        "V_(n)'", "V_(n-1)'", "I_(n)'",  "I_(n-1)'"
    };
    std::cout << std::format(
        "forecast_col_titles.size(): {},  forecast.cols(): "
        "{}, test_labels.cols(): {}\n",
        forecast_col_titles.size(), forecast.cols(), test_labels.cols() );
    Mat<double> forecast_data( forecast.rows(),
                               forecast.cols() + test_labels.cols() );

    forecast_data << test_labels.leftCols( 1 ), forecast,
        test_labels.rightCols( d );

    const auto write_success{ CSV::SimpleCSV::write<double>(
        forecast_path, forecast_data, forecast_col_titles ) };

    if ( !write_success ) {
        std::cerr << std::format( "Unable to write forecast data.\n" );
    }

#endif
#ifdef DOUBLESCROLL
    std::cout << "Running doublescroll." << std::endl;

    // Doublescroll
    const std::filesystem::path doublescroll_train_path{
        "../data/train_data/doublescroll.csv"
    };
    const std::filesystem::path doublescroll_test_path{
        "../data/test_data/doublescroll.csv"
    };
    // clang-format off
    const auto doublescroll_train_csv{ CSV::SimpleCSV(
        /*filename=*/doublescroll_train_path,
        /*col_titles=*/true,
        /*skip_header=*/0,
        /*delim*/ ",",
        /*max_line_size=*/256 ) };
    const auto doublescroll_test_csv{ CSV::SimpleCSV(
        /*filename=*/doublescroll_test_path,
        /*col_titles=*/true,
        /*skip_header=*/0,
        /*delim*/ ",",
        /*max_line_size=*/256 ) };
    // clang-format on

    const auto doublescroll_train_data{ doublescroll_train_csv.atv<double>() };
    const auto doublescroll_test_data{ doublescroll_test_csv.atv<double>() };

    const bool   use_const{ false };
    const Index  d2{ 3 }, k2{ 2 }, s2{ 1 }, p2{ 3 };
    const double alpha{ 5E-2 }, constant{ 1 };
    std::cout << std::format(
        "doublescroll_train_data: {}\n",
        mat_shape_str<double, -1, -1>( doublescroll_train_data ) );
    const FeatureVecShape feature_shape{ { 1, 0 }, { 2, 0 }, { 3, 0 } };

    const auto [doublescroll_train_pair, doublescroll_test_pair] =
        data_split<double>( doublescroll_train_data, doublescroll_test_data,
                            feature_shape, NVAR::warmup_offset( k2, s2 ), 1,
                            UTIL::Standardizer<double>{} );
    const auto [doublescroll_train_samples, doublescroll_train_labels] =
        doublescroll_train_pair;
    const auto [doublescroll_warmup, doublescroll_test_labels] =
        doublescroll_test_pair;

    // Create NVAR model
    NVAR::NVAR<double> doublescroll_test(
        doublescroll_train_samples, doublescroll_train_labels, d2, k2, s2, p2,
        use_const, constant, L2Solver( alpha ), true, { "v1", "v2", "I" },
        "../data/forecast_data/doublescroll_reconstruct.csv" );

    // Forecast
    auto doublescroll_forecast{ doublescroll_test.forecast(
        doublescroll_warmup, doublescroll_test_labels, { 0, 1 } ) };

    // Write data
    const std::filesystem::path doublescroll_forecast_path{
        "../data/forecast_data/"
        "doublescroll_predict.csv"
    };
    const std::vector<std::string> col_titles{ "v1", "v2", "I" };

    const auto write_success{ CSV::SimpleCSV::write<double>(
        doublescroll_forecast_path, doublescroll_forecast, col_titles ) };

    if ( !write_success ) {
        std::cerr << std::format( "Unable to write forecast data.\n" );
    }
#endif
#ifdef HH_MODEL
    const auto base_path{ std::filesystem::absolute(
        std::filesystem::current_path() / ".." ) };

    const std::vector<std::string> files{ "a2t11", "a4t15", "a6t12", "a6t38",
                                          "a8t19" };
    std::vector<std::string>       results_files;

    const bool   use_const{ true };
    const double alpha{ 0.1 }, constant{ 1 };
    const Index  d{ 2 }, k{ 5 }, s{ 20 }, p{ 5 }, data_stride{ 5 }, delay{ 1 };
    std::cout << std::format( "data_stride: {}, delay: {}\n", data_stride,
                              delay );
    std::cout << std::format( "alpha: {}, use_const: {}, constant: {}\n", alpha,
                              use_const ? "true" : "false", constant );
    std::cout << std::format( "d = {}, k = {}, s = {}, p = {}\n", d, k, s, p );

    for ( const auto & file : files ) {
        const auto python_file = base_path / "data" / "decompress_signal.py";
        const auto read_path = base_path / "data" / "hh_data" / file;
        const auto write_path = base_path / "tmp" / ( file + ".csv" );

        std::cout << "Reading from: " << read_path << std::endl;
        std::cout << "Writing to: " << write_path << std::endl;

        std::cout << "Decompressing..." << std::endl;
        const std::string decompress_cmd{ std::format(
            "python3.11 {} {} {}", python_file.string(), read_path.string(),
            write_path.string() ) };
        const auto [decompress_result_code, decompress_msg] =
            exec( decompress_cmd.c_str() );

        std::cout << std::format(
            "{}:\nResult code: {}\n-----\nMessage:\n-----\n{}\nDone.\n", file,
            decompress_result_code, decompress_msg );

        if ( decompress_result_code == 0 /* Success code */ ) {
            std::cout << "Successful decompress" << std::endl;
            // Load data
            std::cout << "Loading csv data." << std::endl;
            // clang-format off
            const auto csv{ CSV::SimpleCSV(
                /*filename=*/write_path,
                /*col_titles=*/true,
                /*skip_header=*/0,
                /*delim*/ ",",
                /*max_line_size=*/256 )
            };
            // clang-format on
            std::cout << "Loading csv into matrix." << std::endl;
            const Mat<double> full_data{ csv.atv<double>( 0, 0 ) };

            // Split data
            std::cout << "Splitting data..." << std::endl;

            const FeatureVecShape shape{ { 0, 0 }, { 1, 0 }, { 2, 0 } };

            const auto [train_pair, test_pair] = data_split<double>(
                /* data */ full_data, /* train_test_ratio */ 0.75,
                /* shape */ shape,
                /*warmup_offset*/NVAR::warmup_offset/* k */ k,
                /* s */ s), /* stride */ data_stride, /* DataProcessor */ UTIL::NullProcessor<double>{} );

            const auto [train_samples, train_labels] = train_pair;
            const auto [test_warmup, test_labels] = test_pair;

            std::cout << std::format(
                "train_samples: {}\ntrain_labels: {}\ntest_warmup: "
                "{}\ntest_labels: {}\n",
                mat_shape_str<double, -1, -1>( train_samples ),
                mat_shape_str<double, -1, -1>( train_labels ),
                mat_shape_str<double, -1, -1>( test_warmup ),
                mat_shape_str<double, -1, -1>( test_labels ) );

            std::cout << "Done." << std::endl;

            // Train NVAR
            std::cout << "Training NVAR..." << std::endl;

            NVAR::NVAR<double> nvar(
                train_samples.rightCols( d ), train_labels.rightCols( d ), d, k,
                s, p, use_const, constant, L2Solver( alpha ), true,
                { "Vmembrane", "Istim" }, "../data/forecast_data/tmp.csv" );

            std::cout << "Done." << std::endl;

            // Forecast forwards
            std::cout << "Forecasting..." << std::endl;

            const auto forecast{ nvar.forecast( test_warmup.rightCols( d ),
                                                test_labels.rightCols( d ),
                                                { 1 } ) };

            std::cout << "Done." << std::endl;

            // Write result file
            std::cout << "Writing result..." << std::endl;
            std::cout << std::format(
                "forecast: {}, test_labels: {}\n",
                mat_shape_str<double, -1, -1>( forecast ),
                mat_shape_str<double, -1, -1>( test_labels ) );

            const std::filesystem::path forecast_path{
                "../data/forecast_data"
            };
            std::cout << std::format( "forecast_path: {}\n",
                                      forecast_path.string() );
            std::cout << std::format( "file: {}, (file+\".csv\"): {}\n", file,
                                      ( file + ".csv" ) );
            const auto write_file{ forecast_path / ( file + ".csv" ) };
            std::cout << std::format( "write_file: {}\n", write_file.string() );

            const std::vector<std::string> col_titles{ "t", "Vmembrane'",
                                                       "Istim'", "Vmembrane",
                                                       "Istim" };

            std::cout << std::format( "col_titles: {}", col_titles.size() );
            std::cout << "col_titles = " << col_titles << std::endl;

            Mat<double> results( forecast.rows(),
                                 test_labels.cols() + forecast.cols() );

            std::cout << std::format(
                "test_labels: {}, forecast: {}\n",
                mat_shape_str<double, -1, -1>( test_labels ),
                mat_shape_str<double, -1, -1>( forecast ) );
            std::cout << std::format(
                "results: {}\n", mat_shape_str<double, -1, -1>( results ) );

            results << test_labels, forecast;

            const auto write_success{ CSV::SimpleCSV::write<double>(
                write_file, results, col_titles ) };

            if ( !write_success ) {
                std::cerr << std::format( "Unable to write forecast data.\n" );
            }
            else {
                results_files.push_back( write_file.string() );
            }

            std::cout << "Done." << std::endl;
        }

        if ( std::filesystem::directory_entry( write_path ).exists() ) {
            // Delete decompressed file
            std::cout << "Deleting decompressed file..." << std::endl;

            const std::string remove_cmd{ std::format( "rm {}",
                                                       write_path.string() ) };

            const auto [result_code, rm_std_output] =
                exec( remove_cmd.c_str() );

            if ( result_code != 0 ) {
                std::cout << "Failed to remove decompressed file." << std::endl;
                std::cout << "Message:\n" << rm_std_output << "\n";
                exit( EXIT_FAILURE );
            }
            std::cout << "Message:\n-----\n" << rm_std_output << "\n-----\n";

            std::cout << "Done.\n" << std::endl;
        }
    }

    /*nlohmann::json file_json;
    file_json["results_files"] = results_files;

    const std::filesystem::path results_file_path{
        "../metadata/results_files.json"
    };
    std::ofstream output( results_file_path );
    output << std::setw( 4 ) << file_json << std::endl;*/

#endif
}
