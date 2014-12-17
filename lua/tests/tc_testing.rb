
class TestTesting < CLIPPTest::TestCase

  parallelize_me!

  include CLIPPTest

  TEST_DIR = File.dirname(__FILE__)

  def test_blocking_api_do_block
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_finished_state(function(tx)

          tx:disableBlocking()
          if tx:isBlockingEnabled(tx) then
            tx:logInfo("Transaction is blockable.")
          else
            tx:logInfo("Transaction is not blockable.")
          end

          tx:enableBlocking()
          if tx:isBlockingEnabled(tx) then
            tx:logInfo("Transaction is blockable.")
          else
            tx:logInfo("Transaction is not blockable.")
          end

          if tx:isBlocked() then
            tx:logInfo("Transaction is blocked.")
          else
            tx:logInfo("Transaction is not blocked.")
          end

          local rc = tx:block()

          if rc == ffi.C.IB_OK then
            tx:logInfo("Blocked transaction.")
          else
            tx:logInfo("Failed to block transaction.")
          end

          return 0
        end)

        return 0
      ''',
      config: '''
      ''',
      default_site_config: '''
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is not blockable.')
    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is blockable.')
    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is not blocked.')
    assert_log_match(/ERROR     - \[tx:.*\]  clipp_error: 403/)
    assert_log_match('INFO      -  LuaAPI - [INFO ] Blocked transaction.')
  end


  def test_ib_list_pairs
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        ibutil = require("ironbee/util")

        s1  = "s1"
        s2  = "s2"
        lst = ffi.new("ib_list_t *[1]")
        mm  = ffi.C.ib_engine_mm_main_get(IB.ib_engine)
        rc  = ffi.C.ib_list_create(lst, mm)
        if rc ~= ffi.C.IB_OK then
          error("Not OK")
        end

        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s1))
        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s2))

        for i,j in ibutil.ib_list_pairs(lst[0], "char *") do
          print("Value ".. ffi.string(j))
        end

      },
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match 'Value s1'
    assert_log_match 'Value s2'
  end

  def test_ib_list_ipairs
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        ibutil = require("ironbee/util")

        s1  = "s1"
        s2  = "s2"
        lst = ffi.new("ib_list_t *[1]")
        mm  = ffi.C.ib_engine_mm_main_get(IB.ib_engine)
        rc  = ffi.C.ib_list_create(lst, mm)
        if rc ~= ffi.C.IB_OK then
          error("Not OK")
        end

        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s1))
        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s2))

        for i,j in ibutil.ib_list_ipairs(lst[0], "char *") do
          print("Value", i, ffi.string(j))
        end

      },
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Value\t1\ts1"
    assert_log_match "Value\t2\ts2"
  end

end
