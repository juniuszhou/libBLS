/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of libBLS.

    libBLS is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libBLS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with libBLS.  If not, see <http://www.gnu.org/licenses/>.

    @file bls_glue.cpp
    @author Oleh Nikolaiev
    @date 2019
*/


#include <fstream>

#include <bls/bls.h>
#include <third_party/json.hpp>

#include <boost/program_options.hpp>

#define EXPAND_AS_STR( x ) __EXPAND_AS_STR__( x )
#define __EXPAND_AS_STR__( x ) #x

static bool g_b_verbose_mode = false;

template<class T>
std::string ConvertToString(T field_elem) {
  mpz_t t;
  mpz_init(t);

  field_elem.as_bigint().to_mpz(t);

  char * tmp = mpz_get_str(NULL, 10, t);
  mpz_clear(t);

  std::string output = tmp;

  return output;
}

void RecoverSignature(const size_t t, const size_t n, const std::vector<std::string>& input) {
  signatures::Bls bls_instance = signatures::Bls(t, n);

  std::vector<size_t> idx(t);
  std::vector<libff::alt_bn128_G1> signature_shares(t);

  for (size_t i = 0; i < t; ++i) {
    std::ifstream data(input[i]);

    nlohmann::json signature;

    data >> signature;

    idx[i] = stoi(signature["index"].get<std::string>()) + 1;

    libff::alt_bn128_G1 signature_share;
    signature_share.X = libff::alt_bn128_Fq(signature["signature"]["X"].get<std::string>().c_str());
    signature_share.Y = libff::alt_bn128_Fq(signature["signature"]["Y"].get<std::string>().c_str());
    signature_share.Z = libff::alt_bn128_Fq(signature["signature"]["Z"].get<std::string>().c_str());

    signature_shares[i] = signature_share;
  }

  std::vector<libff::alt_bn128_Fr> lagrange_coeffs = bls_instance.LagrangeCoeffs(idx);

  libff::alt_bn128_G1 common_signature = bls_instance.SignatureRecover(signature_shares, lagrange_coeffs);

  nlohmann::json outdata;

  outdata["signature"]["X"] = ConvertToString<libff::alt_bn128_Fq>(common_signature.X);
  outdata["signature"]["Y"] = ConvertToString<libff::alt_bn128_Fq>(common_signature.Y);
  outdata["signature"]["Z"] = ConvertToString<libff::alt_bn128_Fq>(common_signature.Z);

  std::cout << outdata.dump(4) << '\n';
}

int main(int argc, const char *argv[]) {
  int r = 1;
  try {
    boost::program_options::options_description desc("Options");
    desc.add_options()
      ("help", "Show this help screen")
      ("version", "Show version number")
      ("t", boost::program_options::value<size_t>(), "Threshold")
      ("n", boost::program_options::value<size_t>(), "Number of participants")
      ("input", boost::program_options::value<std::vector<std::string>>(), "Input file path; if not specified then use standard input")
      ("v", "Verbose mode (optional)")
      ;

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.count("help") || argc <= 1) {
      std::cout
        << "BLS signature verification tool, version " << EXPAND_AS_STR( BLS_VERSION ) << '\n'
        << "Usage:\n"
        << "   " << argv[0] << " --t <threshold> --n <num_participants> [--input <path>] [--v]" << '\n'
        << desc << '\n';
      return 0;
    }
    if (vm.count("version")) {
      std::cout
        << EXPAND_AS_STR( BLS_VERSION ) << '\n';
      return 0;
    }

    if (vm.count("t") == 0)
      throw std::runtime_error( "--t is missing (see --help)" );
    if (vm.count("n") == 0)
      throw std::runtime_error( "--n is missing (see --help)" );

    if (vm.count("v"))
      g_b_verbose_mode = true;

    size_t t = vm["t"].as<size_t>();
    size_t n = vm["n"].as<size_t>();
    if( g_b_verbose_mode )
      std::cout
        << "t = " << t << '\n'
        << "n = " << n << '\n'
        << '\n';

    std::vector<std::string> input;
    if( vm.count("input") ) {
      input = vm["input"].as<std::vector<std::string>>();
      if( g_b_verbose_mode ) {
        std::cout << "input =\n";
        for(auto& elem : input)
          std::cout << elem << '\n';
      }
    }

    RecoverSignature(t, n, input);
    r = 0;

  } catch ( std::exception & ex ) {
    r = 1;
    std::string str_what = ex.what();
    if( str_what.empty() )
      str_what = "exception without description";
    std::cerr << "exception: " << str_what << "\n";
  } catch (...) {
    r = 2;
    std::cerr << "unknown exception\n";
  }

  return r;
}
