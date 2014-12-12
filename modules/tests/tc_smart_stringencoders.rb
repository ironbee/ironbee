URL_TEST_CASES = [

  # These are the base inputs. We check that they equal themselves (are not "damaged").
  [ '../File.txt', '../File.txt'],
  [ '../../File.txt', '../../File.txt'],
  [ '../../../File.txt', '../../../File.txt'],
  [ '../../../../File.txt', '../../../../File.txt'],
  [ '../../../../../File.txt', '../../../../../File.txt'],
  [ '../../../../../../File.txt', '../../../../../../File.txt'],
  [ '../../../../../../../File.txt', '../../../../../../../File.txt'],
  [ '../../../../../../../../File.txt', '../../../../../../../../File.txt'],


  # SIMPLE URL ENCODING
  [ '..%2fFile.txt', '../File.txt'],
  [ '..%2f..%2fFile.txt', '../../File.txt'],
  [ '..%2f..%2f..%2fFile.txt', '../../../File.txt'],
  [ '..%2f..%2f..%2f..%2fFile.txt', '../../../../File.txt'],
  [ '..%2f..%2f..%2f..%2f..%2fFile.txt', '../../../../../File.txt'],
  [ '..%2f..%2f..%2f..%2f..%2f..%2fFile.txt', '../../../../../../File.txt'],
  [ '..%2f..%2f..%2f..%2f..%2f..%2f..%2fFile.txt', '../../../../../../../File.txt'],
  [ '..%2f..%2f..%2f..%2f..%2f..%2f..%2f..%2fFile.txt', '../../../../../../../../File.txt'],
  [ '%2e%2e/File.txt', '../File.txt'],
  [ '%2e%2e/%2e%2e/File.txt', '../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../../../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../../../../../File.txt'],
  [ '%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/%2e%2e/File.txt', '../../../../../../../../File.txt'],
  [ '%2e%2e%2fFile%2etxt', '../File.txt'],
  [ '%2e%2e%2f%2e%2e%2fFile%2etxt', '../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../../../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../../../../../File.txt'],
  [ '%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2fFile%2etxt', '../../../../../../../../File.txt'],
  [ '..%5c..%5cFile.txt', '..\..\File.txt'],
  [ '..%5c..%5c..%5cFile.txt', '..\..\..\File.txt'],
  [ '..%5c..%5c..%5c..%5cFile.txt', '..\..\..\..\File.txt'],
  [ '..%5c..%5c..%5c..%5c..%5cFile.txt', '..\..\..\..\..\File.txt'],
  [ '..%5c..%5c..%5c..%5c..%5c..%5cFile.txt', '..\..\..\..\..\..\File.txt'],
  [ '..%5c..%5c..%5c..%5c..%5c..%5c..%5cFile.txt', '..\..\..\..\..\..\..\File.txt'],
  [ '..%5c..%5c..%5c..%5c..%5c..%5c..%5c..%5cFile.txt', '..\..\..\..\..\..\..\..\File.txt'],
  [ '%2e%2e\File.txt', '..\File.txt'],
  [ '%2e%2e\%2e%2e\File.txt', '..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\..\..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\..\..\..\..\File.txt'],
  [ '%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\%2e%2e\File.txt', '..\..\..\..\..\..\..\..\File.txt'],
  [ '%2e%2e%5cFile.txt', '..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5cFile.txt', '..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\..\..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\..\..\..\..\File.txt'],
  [ '%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5c%2e%2e%5cFile.txt', '..\..\..\..\..\..\..\..\File.txt'],



  ### DOUBLE URL ENCODING

  # DOUBLE percent encoding
  [ '..%252fFile.txt', '../File.txt'],
  [ '..%252f..%252fFile.txt', '../../File.txt'],
  [ '..%252f..%252f..%252fFile.txt', '../../../File.txt'],
  [ '..%252f..%252f..%252f..%252fFile.txt', '../../../../File.txt'],
  [ '..%252f..%252f..%252f..%252f..%252fFile.txt', '../../../../../File.txt'],
  [ '..%252f..%252f..%252f..%252f..%252f..%252fFile.txt', '../../../../../../File.txt'],
  [ '..%252f..%252f..%252f..%252f..%252f..%252f..%252fFile.txt', '../../../../../../../File.txt'],
  [ '..%252f..%252f..%252f..%252f..%252f..%252f..%252f..%252fFile.txt', '../../../../../../../../File.txt'],
  [ '%252e%252e/File.txt', '../File.txt'],
  [ '%252e%252e/%252e%252e/File.txt', '../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../../../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../../../../../File.txt'],
  [ '%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/%252e%252e/File.txt', '../../../../../../../../File.txt'],
  [ '%252e%252e%252fFile.txt', '../File.txt'],
  [ '%252e%252e%252f%252e%252e%252fFile.txt', '../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../../../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../../../../../File.txt'],
  [ '%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252f%252e%252e%252fFile.txt', '../../../../../../../../File.txt'],
  [ '..%255cFile.txt', ''],
  [ '..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255c..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255c..%255c..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255c..%255c..%255c..%255c..%255cFile.txt', ''],
  [ '..%255c..%255c..%255c..%255c..%255c..%255c..%255c..%255cFile.txt', ''],
  [ '%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\%252e%252e\File.txt', ''],
  [ '%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],
  [ '%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255c%252e%252e%255cFile.txt', ''],


  # Double nibble (part of double url encoding)
  [ '..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66..%%32%66File.txt', ''],
  [ '%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66%%32%65%%32%65%%32%66File.txt', ''],
  [ '..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63..%%35%63File.txt', ''],
  [ '%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/%%32%65%%32%65/File.txt', ''],
  [ '%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],
  [ '%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63%%32%65%%32%65%%35%63File.txt', ''],


  # First nibble (part of double url encoding)
  [ '%%32e%%32e/File%%32etxt', '../File.txt'],
  [ '%%32e%%32e/%%32e%%32e/File%%32etxt', '../../File.txt'],
  [ '%%32e%%32e/%%32e%%32e/%%32e%%32e/File%%32etxt', '../../../File.txt'],

  # Second nibble (part of double url encoding)
  [ '%2%65%2%65/File%2%65txt', '../File.txt'],
  [ '%2%65%2%65/%2%65%2%65/File%2%65txt', '../../File.txt'],
  [ '%2%65%2%65/%2%65%2%65/%2%65%2%65/File%2%65txt', '../../../File.txt'],



  # UTF-8 URL encoding
  [ '..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%af..%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt', ''],
  [ '%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%af%c0%ae%c0%ae%c0%afFile.txt', ''],
  [ '..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9c..%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\%c0%ae%c0%ae\File.txt', ''],
  [ '%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],
  [ '%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9c%c0%ae%c0%ae%c1%9cFile.txt', ''],


  # Accepted but invalid UTF-8 URL encoding
  [ '..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2f..%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/%c0%2e%c0%2e/File.txt', ''],
  [ '%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2f%c0%2e%c0%2e%c0%2fFile.txt', ''],
  [ '..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5c..%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\%c0%2e%c0%2e\File.txt', ''],
  [ '%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],
  [ '%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5c%c0%2e%c0%2e%c0%5cFile.txt', ''],



  # UTF-8 URL encoding + URL encoding
  [ '..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25af..%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/%25c0%25ae%25c0%25ae/File.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25af%25c0%25ae%25c0%25ae%25c0%25afFile.txt', ''],
  [ '..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259c..%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\%25c0%25ae%25c0%25ae\File.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],
  [ '%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259c%25c0%25ae%25c0%25ae%25c1%259cFile.txt', ''],


  # %u UNICODE URL encoding
  [ '..%u2215File.txt', ''],
  [ '..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215File.txt', ''],
  [ '..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215..%u2215File.txt', ''],
  [ '%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/%uff0e%uff0e/File.txt', ''],
  [ '%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215%uff0e%uff0e%u2215File.txt', ''],
  [ '..%u2216File.txt', ''],
  [ '..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216..%u2216File.txt', ''],
  [ '..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8..%uEFC8File.txt', ''],
  [ '..%uF025File.txt', ''],
  [ '..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025File.txt', ''],
  [ '..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025..%uF025File.txt', ''],
  [ '%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\%uff0e%uff0e\File.txt', ''],
  [ '%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
  [ '%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216%uff0e%uff0e%u2216File.txt', ''],
] # End of test cases

HEX_TEST_CASES = [
  # HEX encoding
  [ '..0x2fFile.txt', '../File.txt'],
  [ '..0x2f..0x2fFile.txt', '../../File.txt'],
  [ '..0x2f..0x2f..0x2fFile.txt', '../../../File.txt'],
  [ '..0x2f..0x2f..0x2f..0x2fFile.txt', '../../../../File.txt'],
  [ '..0x2f..0x2f..0x2f..0x2f..0x2fFile.txt', '../../../../../File.txt'],
  [ '..0x2f..0x2f..0x2f..0x2f..0x2f..0x2fFile.txt', '../../../../../../File.txt'],
  [ '..0x2f..0x2f..0x2f..0x2f..0x2f..0x2f..0x2fFile.txt', '../../../../../../../File.txt'],
  [ '..0x2f..0x2f..0x2f..0x2f..0x2f..0x2f..0x2f..0x2fFile.txt', '../../../../../../../../File.txt'],
  [ '0x2e0x2e/File.txt', '../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/File.txt', '../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../../../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../../../../../File.txt'],
  [ '0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/0x2e0x2e/File.txt', '../../../../../../../../File.txt'],
  [ '0x2e0x2e0x2fFile.txt', '../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../../../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../../../../../File.txt'],
  [ '0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2f0x2e0x2e0x2fFile.txt', '../../../../../../../../File.txt'],
  [ '..0x5cFile.txt', ''],
  [ '..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5c..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5c..0x5c..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5c..0x5c..0x5c..0x5c..0x5cFile.txt', ''],
  [ '..0x5c..0x5c..0x5c..0x5c..0x5c..0x5c..0x5c..0x5cFile.txt', ''],
  [ '0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\0x2e0x2e\File.txt', ''],
  [ '0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
  [ '0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5c0x2e0x2e0x5cFile.txt', ''],
]

class TestSmartStringEncoders < CLIPPTest::TestCase
  include CLIPPTest

  # Simple test to ensure that \x decode is working.
  def test_decode_x
    base_hex_decode("\\x20\\\\x20", ' \\ ')
  end

  # Simple test to ensure that % decode is working.
  def test_decode_pct
    base_url_path_decode("%20%%20", ' % ')
  end

  URL_TEST_CASES.each_with_index do |c, i|
    input, result = c
    if result == ''
      next
    end

    class_eval """
      def test_url_decode_#{i}()
        base_url_path_decode(*URL_TEST_CASES[#{i}])
      end
    """
  end

  HEX_TEST_CASES.each_with_index do |c, i|
    input, result = c
    if result == ''
      next
    end

    class_eval """
      def test_hex_decode_#{i}()
        base_hex_decode(*HEX_TEST_CASES[#{i}])
      end
    """
  end

  private

  def base_url_path_decode(input, result)
    clipp(
      modhtp: true,
      modules: %w[ smart_stringencoders ],
      default_site_config: '''
        Rule A.smart_url_hex_decode().smart_url_hex_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
      config: 'InitVar A "%s"'%[ input ],
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: %s'%[ result ]
  end

  def base_hex_decode(input, result)
    clipp(
      modhtp: true,
      modules: %w[ smart_stringencoders ],
      default_site_config: '''
        Rule A.smart_hex_decode().smart_hex_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
      config: 'InitVar A "%s"'%[ input ],
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: %s'%[ result ]
  end

  public

  def test_html_decode_simple()
    clipp(
      modhtp: true,
      modules: %w[ smart_stringencoders ],
      default_site_config: '''
        Rule A.smart_html_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
      config: 'InitVar A "hi&lt;how are you&gt;?"',
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: hi<how are you>?'
  end

  def test_url_hex_decode_list()
    clipp(
      modhtp: true,
      modules: %w[ persistence_framework init_collection smart_stringencoders ],
      config: 'InitCollection A vars: "a=%61" "b=%62"',
      default_site_config: '''
        Rule A.smart_url_hex_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: a'
    assert_log_match 'clipp_print [A]: b'
  end

  def test_hex_decode_list()
    clipp(
      modhtp: true,
      modules: %w[ persistence_framework init_collection smart_stringencoders ],
      config: 'InitCollection A vars: "a=\x61" "b=\x62"',
      default_site_config: '''
        Rule A.smart_hex_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: a'
    assert_log_match 'clipp_print [A]: b'
  end

  def test_html_decode_list()
    clipp(
      modhtp: true,
      modules: %w[ persistence_framework init_collection smart_stringencoders ],
      config: 'InitCollection A vars: "a=&lt;how are you&gt;?" "b=&lt;how are you&gt;?"',
      default_site_config: '''
        Rule A.smart_html_decode() @clipp_print A id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo\n\n")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: <how are you>?'
  end
end


