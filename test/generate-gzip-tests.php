#!/usr/bin/env php
<?

/*

ZLIB Compressed Data Format Specification version 3.3
http://www.ietf.org/rfc/rfc1950.txt

DEFLATE Compressed Data Format Specification version 1.3
http://www.ietf.org/rfc/rfc1951.txt

GZIP file format specification version 4.3
http://www.ietf.org/rfc/rfc1952.txt

*/

class GzipTest {

  private $compressionMethod = 0x08;

  private $forcedFlags = false;

  private $filename = false;
  
  private $comment = false;
  
  private $extra = false;
  
  private $textFlag = false;
  
  private $useHeaderCrc = false;
  
  private $crc32 = false;
  
  private $isize = false;
  
  private $data = "The five boxing wizards jump quickly.";
  
  private $xfl = 0;
  
  public function setCompressionMethod($m) {
    $this->compressionMethod = $m;
  }
  
  public function setCrc32($crc) {
    $this->crc32 = $crc;
  }
  
  public function setInputSize($len) {
    $this->isize = $len;
  }
  
  public function setXfl($xfl) {
    $this->xfl = $xfl;
  }
  
  public function setFilename($filename) {
    $this->filename = $filename;
  }
  
  public function setComment($comment) {
    $this->comment = $comment;
  }
  
  public function setExtra($extra) {
    $this->extra = $extra;
  }
  
  public function setTextFlag($b) {
    $this->textFlag = $b;
  }
  
  public function useHeaderCrc($b) {
    $this->useHeaderCrc = $b;
  }
  
  public function setFlags($f) {
    $this->forcedFlags = $f;
  }
  
  public function getFlags() {
    if ($this->forcedFlags !== false) {
      return $this->forcedFlags;
    }
    
    $flags = 0;
    
    // FTEXT
    if ($this->textFlag) {
      $flags = $flags | 0x01;
    }
    
    // FHCRC
    if ($this->useHeaderCrc) {
      $flags = $flags | 0x02;
    }
    
    // FEXTRA
    if ($this->extra !== false) {
      $flags = $flags | 0x04;
    }
    
    // FNAME
    if ($this->filename !== false) {
      $flags = $flags | 0x08;
    }
    
    // FCOMMENT
    if ($this->comment !== false) {
      $flags = $flags | 0x16;
    }
    
    return $flags;
  }
  
  public function setData($data) {
    $this->data = $data;
  }
  
  public function writeTo($filename) {
    $fp = fopen($filename, "w+");
    $this->write($fp);
    fclose($fp);
  }

  public function write($fp) {
    // header (ID1 + ID2)
    fwrite($fp, "\x1f\x8b");
    
    // compression method (CM)
    fwrite($fp, pack("C", $this->compressionMethod));
    
    // flags (FLG)
    fwrite($fp, pack("C", $this->getFlags()));
    
    // mtime (MTIME)
    fwrite($fp, "\x9c\x54\xf4\x50");
    
    // extra flags (XFL)
    fwrite($fp, pack("C", $this->xfl));
    
    // operating system (OS)
    fwrite($fp, "\xff");
    
    // FEXTRA
    if ($this->extra !== false) {
      fwrite($fp, pack("v", strlen($this->extra)));
      fwrite($fp, $this->extra);
    }
    
    // FNAME
    if ($this->filename !== false) {
      fwrite($fp, $this->filename);
      fwrite($fp, "\x00");
    }
    
    // FCOMMENT
    if ($this->comment !== false) {
      fwrite($fp, $this->comment);
      fwrite($fp, "\x00");
    }
    
    // FHCRC
    if ($this->useHeaderCrc) {
      // Clearly, this is not the real CRC16, use below if a real one is needed:
      // http://stackoverflow.com/questions/14018508/how-to-calculate-crc16-in-php
      fwrite($fp, "\x6e\x37");
    }
    
    // compressed blocks
    $compressedData = gzcompress($this->data);
    // The gzcompress() function does not produce output that's fully compatible with gzip,
    // so we need to strip out the extra data: remove 2 bytes from the beginning
    // (CMF and FLG) and 4 bytes from the end (Adler CRC).
    $compressedData = substr($compressedData, 2, strlen($compressedData) - 6);
    fwrite($fp, $compressedData);
    
    // CRC32
    if ($this->crc32 === false) {
      fwrite($fp, pack("V", crc32($this->data)));
    } else {
      fwrite($fp, pack("V", $this->crc32));
    }
    
    // uncompressed size (ISIZE)
    if ($this->isize === false) {
      fwrite($fp, pack("V", strlen($this->data)));
    } else {
      fwrite($fp, pack("V", $this->isize));
    }
  }
}

// 01: minimal file
$gz = new GzipTest();
$gz->writeTo("gztest-01-minimal.gz");

// 02: with FNAME
$gz = new GzipTest();
$gz->setFilename("file.txt");
$gz->writeTo("gztest-02-fname.gz");

// 03: with FCOMMENT
$gz = new GzipTest();
$gz->setComment("COMMENT");
$gz->writeTo("gztest-03-fcomment.gz");

// 04: with FHCRC
$gz = new GzipTest();
$gz->useHeaderCrc(true);
$gz->writeTo("gztest-04-fhcrc.gz");

// 05: with FEXTRA
$gz = new GzipTest();
$gz->setExtra("EXTRA");
$gz->writeTo("gztest-05-fextra.gz");

// 06: with FTEXT
$gz = new GzipTest();
$gz->setTextFlag(true);
$gz->writeTo("gztest-06-ftext.gz");

// 07: with FRESERVED1
$gz = new GzipTest();
$gz->setFlags($gz->getFlags() | 0x20);
$gz->writeTo("gztest-07-freserved1.gz");

// 08: with FRESERVED2
$gz = new GzipTest();
$gz->setFlags($gz->getFlags() | 0x40);
$gz->writeTo("gztest-08-freserved2.gz");

// 09: with FRESERVED3
$gz = new GzipTest();
$gz->setFlags($gz->getFlags() | 0x80);
$gz->writeTo("gztest-09-freserved3.gz");

// 10: Two parts (compressed streams) 
$gz = new GzipTest();
$fp = fopen("gztest-10-multipart.gz", "w+");
$gz->setFilename("file1.txt");
$gz->write($fp);
$gz->setData("The quick brown fox jumps over the lazy dog.");
$gz->setFilename("file2.txt");
$gz->write($fp);
fclose($fp);

// 11: Invalid compression method
$gz = new GzipTest();
$gz->setCompressionMethod(0x07);
$gz->writeTo("gztest-11-invalid-method.gz");

// 12: Invalid CRC32
$gz = new GzipTest();
$gz->setCrc32(0xffffffff);
$gz->writeTo("gztest-12-invalid-crc32.gz");

// 13: Invalid ISIZE
$gz = new GzipTest();
$gz->setData("Grumpy Wizards make toxic brew for the Evil Queen and Jack.");
$gz->setInputSize(0x10);
$gz->writeTo("gztest-13-invalid-isize.gz");

// 14: Invalid extra flags (XFL)
$gz = new GzipTest();
$gz->setXfl(0xff);
$gz->writeTo("gztest-14-invalid-xfl.gz");

?>
