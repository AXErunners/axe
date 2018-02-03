AM_CPPFLAGS =  $(INCLUDES) \
  -I$(top_builddir)/src/obj \
  $(BDB_CPPFLAGS) \
  $(BOOST_CPPFLAGS) $(BOOST_INCLUDES)
AM_CPPFLAGS += $(LEVELDB_CPPFLAGS)
AM_LDFLAGS = $(PTHREAD_CFLAGS)

AM_CPPFLAGS += -I$(top_srcdir)/src/leveldb/helpers/memenv \
  -I$(builddir)

AM_CPPFLAGS += -I$(builddir)

BITCOIN_CONFIG_INCLUDES=-I$(builddir)/config
BITCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BITCOIN_CONFIG_INCLUDES) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS)

noinst_LIBRARIES = \
  libbitcoin_server.a \
  libbitcoin_common.a \
  libbitcoin_cli.a
if ENABLE_WALLET
noinst_LIBRARIES += libbitcoin_wallet.a
endif

bin_PROGRAMS =
TESTS =

if BUILD_BITCOIND
  bin_PROGRAMS += axed
endif

if BUILD_BITCOIN_CLI
  bin_PROGRAMS += bitcoin-cli
endif

.PHONY: FORCE
# bitcoin core #
BITCOIN_CORE_H = \
  addrman.h \
  alert.h \
  allocators.h \
  base58.h \
  bignum.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  bloom.h \
  checkpoints.h \
  checkpointsync.h \
  checkqueue.h \
  clientversion.h \
  compat.h \
  crypter.h \
  db.h \
  hash.h \
  init.h \
  key.h \
  kernel.h \
  keystore.h \
  txdb-leveldb.h \
  limitedmap.h \
  main.h \
  mruset.h \
  netbase.h \
  net.h \
  protocol.h \
  script.h \
  serialize.h \
  sync.h \
  threadsafety.h \
  txdb.h \
  ui_interface.h \
  uint256.h \
  util.h \
  version.h \
  walletdb.h \
  wallet.h
  zerocoin/Accumulator.h \
  zerocoin/AccumulatorProofOfKnowledge.h \
  zerocoin/Coin.h \
  zerocoin/CoinSpend.h \
  zerocoin/Commitment.h \
  zerocoin/ParamGeneration.h \
  zerocoin/Params.h \
  zerocoin/SerialNumberSignatureOfKnowledge.h \
  zerocoin/SpendMetaData.h \
  zerocoin/ZeroTest.h \
  zerocoin/Zerocoin.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libbitcoin_common_a-version.$(OBJEXT): obj/build.h

libbitcoin_server_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  rpcserver.cpp \
  bloom.cpp \
  checkpoints.cpp \
  checkpointsync.cpp \
  crypter.cpp \
  init.cpp \
  kernel.cpp \
  keystore.cpp \
  txdb-leveldb.cpp\
  main.cpp \
  net.cpp \
  noui.cpp \
  rpcblockchain.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  txdb.cpp \
  zerocoin/Accumulator.cpp \
  zerocoin/AccumulatorProofOfKnowledge.cpp \
  zerocoin/Coin.cpp \
  zerocoin/CoinSpend.cpp \
  zerocoin/Commitment.cpp \
  zerocoin/ParamGeneration.cpp \
  zerocoin/Params.cpp \
  zerocoin/SerialNumberSignatureOfKnowledge.cpp \
  zerocoin/SpendMetaData.cpp \
  zerocoin/ZeroTest.cpp

  $(JSON_H) \
  $(BITCOIN_CORE_H)

libbitcoin_wallet_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_wallet_a_SOURCES = \
  db.cpp \
  rpcdump.cpp \
  rpcmining.cpp \
  rpcminting.cpp \
  script.cpp \
  kernelrecord.cpp \
  wallet.cpp \
  walletdb.cpp \
  rpcwallet.cpp \
  $(BITCOIN_CORE_H)

libbitcoin_common_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_common_a_SOURCES = \
  hash.cpp \
  key.cpp \
  netbase.cpp \
  protocol.cpp \
  rpcprotocol.cpp \
  sync.cpp \
  util.cpp \
  version.cpp \
  $(BITCOIN_CORE_H)

libbitcoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(BITCOIN_CORE_H)

nodist_libbitcoin_common_a_SOURCES = $(srcdir)/obj/build.h
#

# axed binary #
axed_LDADD = \
  libbitcoin_server.a \
  libbitcoin_cli.a \
  libbitcoin_common.a \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(BOOST_LIBS)
if ENABLE_WALLET
axed_LDADD += libbitcoin_wallet.a
endif
axed_SOURCES = bitcoind.cpp
axed_LDFLAGS = $(AM_LDFLAGS)

if TARGET_WINDOWS
axed_SOURCES += bitcoind-res.rc
endif

axed_LDADD += $(BDB_LIBS)
axed_CPPFLAGS = $(BITCOIN_INCLUDES)

# bitcoin-cli binary #
bitcoin_cli_LDADD = libbitcoin_cli.a libbitcoin_common.a $(BOOST_LIBS)
bitcoin_cli_SOURCES = bitcoin-cli.cpp
bitcoin_cli_LDFLAGS = $(AM_LDFLAGS)
bitcoin_cli_CPPFLAGS = $(BITCOIN_INCLUDES)

if TARGET_WINDOWS
bitcoin_cli_SOURCES += bitcoin-cli-res.rc
endif

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
 -rm -f config.h

.rc.o:
	@test -f $(WINDRES) && $(WINDRES) -i $< -o $@ || \
	  echo error: could not build $@

.mm.o:
	$(OBJC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

LIBAXE_SERVER=libbitcoin_server.a
LIBAXE_WALLET=libbitcoin_wallet.a
LIBAXE_COMMON=libbitcoin_common.a
LIBbitcoin_cli=libbitcoin_cli.a
LIBAXEQT=qt/libbitcoinqt.a

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
