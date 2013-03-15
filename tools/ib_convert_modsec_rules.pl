#!/usr/bin/perl
#####################################################################
# Convert ModSecurity rules to the IronBee Rule Lang. Reads stdin
# and writes to stdout.
#
# WARNING: There are still many rules this cannot convert and some
#          complex logic may be lost (especially skip/skipAfter).
#
# Known Issues:
#
# * IronBee just does not support skip/skipAfter and probably
#   never will. This is a design decision.
# * There is no proxy support in IronBee.
# * Best effort is used to convert field names, but there are
#   some that do not translate.
# * Unknown operators are converted to !@nop, which will
#   never return true (rule will never trigger actions).
# * SecMarker does not have phase, so cannot be converted to
#   IronBee RuleMarker.
# * Metadata actions are converted to tags where there is
#   no direct translation: EX: maturity:5 => tag:maturity/5
#
# TODO:
#
# * Add support to translate to Lua rule lang syntax.
#
# EX: To read msrules.conf, convert and save to ibrules.conf
#
#   ib_convert_modsec_rules.pl <msrules.conf >ibrules.conf
#
#####################################################################

package IB::Rule::Field;
use Text::Balanced qw( extract_quotelike );
use strict;
sub new() {
    my($type,$name,@tfn) = @_;
    my($selector,$prefix,$neg);
    if (defined($name) and $name =~ m/^([&#!]?)([^\.:]+)(?::(.*))?/) {
        ($prefix,$name,$selector) = ($1,$2,$3);

        # Some prefixes are implied tfns, others are modifiers
        if ($prefix eq "&") {
            unshift @tfn, "count";
        }
        elsif ($prefix eq "#") {
            unshift @tfn, "length";
        }
        elsif ($prefix eq "!") {
            $neg = $prefix;
        }

        my $extra;
        if ($selector) {
            ($selector,$extra) = extract_quotelike($selector);
            if (!defined($selector)) {
                ($selector,$extra) = split(/\./,$extra, 2);
            }
        }
        if ($extra) {
            $extra =~ s/^\s*\.//;
            $extra =~ s/\(\)\s*$//;
            push @tfn, split(/\(\)\./, $extra);
        }
    }
    my $self = {
        name => $name,
        neg => $neg,
        selector => $selector,
        tfn => \@tfn,
    };
    bless($self, $type);
    return $self;
}
sub toIBRuleLang() {
    my($self) = @_;
    my $res = $self->{name} .
              (defined($self->{selector}) ? ":".$self->{selector} : "");
    if (@{ $self->{tfn} || [] } > 0) {
       $res .= ".".join("().", @{ $self->{tfn} || [] })."()";
    }

    return $res;
}

package IB::Rule::Op;
use strict;
sub new() {
    if (@_ == 1) {
        return undef;
    }
    my($type,$neg,$name,$arg) = @_;
    $neg = ($neg ? "!" : "");
    my $self = {
        neg => $neg,
        name => $name,
        arg => $arg,
    };
    bless($self, $type);
    return $self;
}
sub toIBRuleLang() {
    my($self) = @_;
    sprintf(
        "%s\@%s \"%s\"",
        $self->{neg},
        $self->{name},
        (defined($self->{arg}) ? $self->{arg} : ""),
    );
}

package IB::Rule::Modifier;
use strict;
sub new() {
    my($type,$name,$arg) = @_;
    my $self = {
        name => $name,
        arg => $arg,
    };
    bless($self, $type);
    return $self;
}
sub toIBRuleLang() {
    my($self) = @_;
    return $self->{name} .
           (defined($self->{arg}) ? ":".$self->{arg} : "");
}

package IB::Rule;
use strict;
sub new() {
    my($type,$directive,$field_raw,$op_raw,$modifier_raw) = @_;
    my $self = {
        raw => undef,
        directive => $directive,
        field => {
            raw => $field_raw,
            parsed => [],
        },
        op => {
            raw => $op_raw,
            parsed => undef,
        },
        modifier => {
            raw => $modifier_raw,
            parsed => [],
        },
        chains => [],
    };
    bless($self, $type);
    return $self;
}
sub toIBRuleLang() {
    my($self) = @_;
    my $ret;
    if ($self->{directive} eq "Action") {
        $ret = sprintf(
            "%s %s",
            $self->{directive},
            (join " ", map { "\"" . $_->toIBRuleLang() . "\"" } @{ $self->{modifier}{parsed} || [] }),
        );
    }
    if ($self->{directive} eq "RuleMarker") {
        $ret = sprintf(
            "# not-implemented # %s %s",
            $self->{directive},
            (join " ", map { "\"" . $_->toIBRuleLang() . "\"" } @{ $self->{modifier}{parsed} || [] }),
        );
    }
    else {
        $ret = sprintf(
            "%s %s %s %s",
            $self->{directive},
            (join " ", map { "\"" . $_->toIBRuleLang() . "\"" } @{ $self->{field}{parsed} }),
            $self->{op}{parsed}->toIBRuleLang(),
            (join " ", map { "\"" . $_->toIBRuleLang() . "\"" } @{ $self->{modifier}{parsed} || [] }),
        );
    }
    return $ret;
}

package main;
use strict;

use Parse::RecDescent;
use Data::Dumper;

$Data::Dumper::Pad = "# ";

# Enable warnings within the Parse::RecDescent module.

$::RD_ERRORS = 1; # Make sure the parser dies when it encounters an error
$::RD_WARN   = 1; # Enable warnings. This will warn on unused rules &c.
$::RD_HINT   = 1; # Give out hints to help fix problems.

my $rule_parser = <<'_EOGRAMMER_';
    # Starting rule to breakdown rule lines into raw components for further parsing
    rule_line: sec_rule
             | sec_action
             | sec_marker

    # SecRule directive
    sec_rule: "SecRule" field_raw op_raw modifier_raw { new IB::Rule("Rule", $item[2], $item[3], $item[4]) }
            | "SecRule" field_raw op_raw { new IB::Rule("Rule", $item[2], $item[3], "") }

    # SecAction directive (converts to Rule)
    sec_action: "SecAction" modifier_raw { new IB::Rule("Action", "NULL", "\@nop 0", $item[2]) }

    # SecMarker directive (converts to RuleMarker)
    sec_marker: "SecMarker" modifier_raw { new IB::Rule("RuleMarker", "NULL", "\@nop 0", "id:$item[2]") }

    field_raw: raw_tok

    op_raw   : raw_tok

    modifier_raw: raw_tok

    raw_tok  : <perl_quotelike> { $item[1][2] }
             | /\S+/

    fields   : field_toks
             { my @fields = map { new IB::Rule::Field( main::field_modsec2ib($_) ) } @{ $item[1] || [] }; \@fields }

    field_toks: field_tok /\|/ field_toks { [ $item[1], @{ $item[3] || [] } ] }
              | field_tok { [ $item[1] ] }

    field_tok : /[^\|]+/

    operator  : /\!?/ "\@" op_tok /.*/ { new IB::Rule::Op( main::op_modsec2ib($item[1], $item[3], $item[4]) ) }
              | /\!?/ regex_tok { new IB::Rule::Op($item[1], "rx", $item[2]) }

    op_tok    : /[a-zA-Z0-9_]+/

    regex_tok : /.+/

    actions   : actions_parsed /,/ actions { [ $item[1], @{ $item[3] || [] } ] }
              | actions_parsed { [ $item[1] ] }

    actions_parsed: /[^,:]+/ /:/ action_value { new IB::Rule::Modifier( main::action_modsec2ib($item[1], $item[3]) ) }
              | /[^,:]+/ { new IB::Rule::Modifier( main::action_modsec2ib($item[1]) ) }

    action_value: <perl_quotelike> { $item[1][2] }
              | /[^,]+/

_EOGRAMMER_

# Field mapping: modsec => ib
my %FIELD_MAP = (
    ARGS => sub { $_[0] },
#    ARGS_COMBINED_SIZE => sub { "NULL" },
    ARGS_GET => sub { "REQUEST_URI_PARAMS" },
#    ARGS_GET_NAMES => sub { "NULL" },
#    ARGS_NAMES => sub { "NULL" },
    ARGS_POST => sub { "REQUEST_BODY_PARAMS" },
#    ARGS_POST_NAMES => sub { "NULL" },
    AUTH_TYPE => sub { $_[0] },
#    DURATION => sub { "NULL" },
#    ENV => sub { "NULL" },
#    FILES => sub { "NULL" },
#    FILES_COMBINED_SIZE => sub { "NULL" },
#    FILES_NAMES => sub { "NULL" },
#    FILES_SIZES => sub { "NULL" },
#    FILES_TMPNAMES => sub { "NULL" },
    GEO => sub { "GEOIP" },
#    HIGHEST_SEVERITY => sub { "NULL" },
#    INBOUND_DATA_ERROR => sub { "NULL" },
    MATCHED_VAR => sub { "FIELD" },
#    MATCHED_VARS => sub { "NULL" },
    MATCHED_VAR_NAME => sub { "FIELD_NAME" },
#    MATCHED_VARS_NAMES => sub { "NULL" },
#    MODSEC_BUILD => sub { "NULL" },
#    MULTIPART_CRLF_LF_LINES => sub { "NULL" },
#    MULTIPART_STRICT_ERROR => sub { "NULL" },
#    MULTIPART_UNMATCHED_BOUNDARY => sub { "NULL" },
#    OUTBOUND_DATA_ERROR => sub { "NULL" },
#    PATH_INFO => sub { "NULL" },
#    PERF_COMBINED => sub { "NULL" },
#    PERF_GC => sub { "NULL" },
#    PERF_LOGGING => sub { "NULL" },
#    PERF_PHASE1 => sub { "NULL" },
#    PERF_PHASE2 => sub { "NULL" },
#    PERF_PHASE3 => sub { "NULL" },
#    PERF_PHASE4 => sub { "NULL" },
#    PERF_PHASE5 => sub { "NULL" },
#    PERF_RULES => sub { "NULL" },
#    PERF_SREAD => sub { "NULL" },
#    PERF_SWRITE => sub { "NULL" },
    QUERY_STRING => sub { "REQUEST_URI_QUERY" },
    REMOTE_ADDR => sub { $_[0] },
    REMOTE_HOST => sub { $_[0] },
    REMOTE_PORT => sub { $_[0] },
#    REMOTE_USER => sub { "NULL" },
#    REQBODY_ERROR => sub { "NULL" },
#    REQBODY_ERROR_MSG => sub { "NULL" },
#    REQBODY_PROCESSOR => sub { "NULL" },
#    REQUEST_BASENAME => sub { "NULL" },
#    REQUEST_BODY => sub { "NULL" },
#    REQUEST_BODY_LENGTH => sub { "NULL" },
    REQUEST_COOKIES => sub { $_[0] },
#    REQUEST_COOKIES_NAMES => sub { "NULL" },
    REQUEST_FILENAME => sub { $_[0] },
    REQUEST_HEADERS => sub { $_[0] },
#    REQUEST_HEADERS_NAMES => sub { "NULL" },
    REQUEST_LINE => sub { $_[0] },
    REQUEST_METHOD => sub { $_[0] },
    REQUEST_PROTOCOL => sub { $_[0] },
    REQUEST_URI => sub { $_[0] },
    REQUEST_URI_RAW => sub { $_[0] },
#    RESPONSE_BODY => sub { "NULL" },
#    RESPONSE_CONTENT_LENGTH => sub { "NULL" },
    RESPONSE_CONTENT_TYPE => sub { $_[0] },
    RESPONSE_HEADERS => sub { $_[0] },
#    RESPONSE_HEADERS_NAMES => sub { "NULL" },
    RESPONSE_PROTOCOL => sub { $_[0] },
    RESPONSE_STATUS => sub { $_[0] },
#    RULE => sub { "NULL" },
#    SCRIPT_BASENAME => sub { "NULL" },
#    SCRIPT_FILENAME => sub { "NULL" },
#    SCRIPT_GID => sub { "NULL" },
#    SCRIPT_GROUPNAME => sub { "NULL" },
#    SCRIPT_MODE => sub { "NULL" },
#    SCRIPT_UID => sub { "NULL" },
#    SCRIPT_USERNAME => sub { "NULL" },
    SERVER_ADDR => sub { $_[0] },
#    SERVER_NAME => sub { "NULL" },
    SERVER_PORT => sub { $_[0] },
#    SESSION => sub { "NULL" },
#    SESSIONID => sub { "NULL" },
#    STREAM_INPUT_BODY => sub { "NULL" },
#    STREAM_OUTPUT_BODY => sub { "NULL" },
#    TIME => sub { "NULL" },
#    TIME_DAY => sub { "NULL" },
#    TIME_EPOCH => sub { "NULL" },
#    TIME_HOUR => sub { "NULL" },
#    TIME_MIN => sub { "NULL" },
#    TIME_MON => sub { "NULL" },
#    TIME_SEC => sub { "NULL" },
#    TIME_WDAY => sub { "NULL" },
#    TIME_YEAR => sub { "NULL" },
    TX => sub {
        # Modsec uses TX:[0-9] for captures
        if ($_[1] =~ m/^:\d($|\D)/) {
            return "CAPTURE";
        }
        else {
            return $_[0];
        }
    },
#    UNIQUE_ID => sub { "NULL" },
#    URLENCODED_ERROR => sub { "NULL" },
#    USERID => sub { "NULL" },
#    USERAGENT_IP => sub { "NULL" },
#    WEBAPPID => sub { "NULL" },
#    WEBSERVER_ERROR_LOG => sub { "NULL" },
#    XML => sub { "NULL" },
);

sub field_modsec2ib {
    my($arg) = @_;
    my($prefix,$name,$extra) = ($arg =~ m/^([&#\!]?)([^:]+)(.*)/);

    $name =~ tr/[a-z]/[A-Z]/;

    if (!exists $FIELD_MAP{$name}) {
        return $arg;
    }

    my $res = $FIELD_MAP{$name}($name,$extra);
    if (!defined $res) {
        return "NULL";
    }

    return $prefix.$res.$extra;
}

# Actions mapping: modsec => ib
my %ACTION_MAP = (
    accuracy => sub { ("confidence", $_[1]) },
    allow => sub { ($_[0], $_[1]) },
    append => sub { () },
    auditlog => sub { ("event") },
    block => sub { ($_[0], $_[1]) },
    capture => sub { ($_[0], $_[1]) },
    chain => sub { ($_[0], $_[1]) },
    ctl => sub { () },
    deny => sub { ("block", $_[1]) },
    deprecatevar => sub { () },
    drop => sub { ("block", $_[1]) },
    exec => sub { () },
    expirevar => sub { () },
    id => sub { ($_[0], $_[1]) },
    initcol => sub { () },
    log => sub { ("event") },
    logdata => sub {
        my($action,$val) = @_;

        # Change collection syntax
        $val =~ s/(tx|ip|resource|rule)\./$1:/gi;

        # Translate field names
        $val =~ s/\%{([^}]+)}/"\%{".field_modsec2ib($1)."}"/ge;

        return ($action, $val);
    },
    maturity => sub { ("tag", $_[0]."/".$_[1]) },
    msg => sub {
        my($action,$val) = @_;

        # Change collection syntax
        $val =~ s/(tx|ip|resource|rule)\./$1:/gi;

        # Translate field names
        $val =~ s/\%{([^}]+)}/"\%{".field_modsec2ib($1)."}"/ge;

        return ($action, $val);
    },
    multiMatch => sub { () },
    noauditlog => sub { () },
    nolog => sub { () },
    pass => sub { () },
    pause => sub { () },
    phase => sub {
        if ($_[1] eq "1") {
            return ($_[0], "REQUEST_HEADER");
        }
        elsif ($_[1] eq "2") {
            return ($_[0], "REQUEST");
        }
        elsif ($_[1] eq "3") {
            return ($_[0], "RESPONSE_HEADER");
        }
        elsif ($_[1] eq "4") {
            return ($_[0], "RESPONSE");
        }
        elsif ($_[1] eq "5") {
            return ($_[0], "REQUEST_HEADER");
        }
        else {
            return ($_[0], "REQUEST");
        }
    },
    prepend => sub { () },
    proxy => sub { () },
    redirect => sub { () },
    rev => sub { ($_[0], $_[1]) },
    sanitiseArg => sub { () },
    sanitiseMatched => sub { () },
    sanitiseMatchedBytes => sub { () },
    sanitiseRequestHeader => sub { () },
    sanitiseResponseHeader => sub { () },
    severity => sub { ($_[0], $_[1]) },
    setuid => sub { ("tag", "uid/".$_[1]) },
    setrsc => sub { ("tag", "rsc/".$_[1]) },
    setsid => sub { ("tag", "env/".$_[1]) },
    setenv => sub { ("tag", "/".$_[1]) },
    setvar => sub {
        my($action,$val) = @_;
        my($name,$op,$val) = ($val =~ m/^([^=]+)([-+=]+)(.*)/);

        # Reverse =+ to +=
        $op =~ s/(=)([-+])/$2$1/;

        # Change collection syntax
        $name =~ s/(tx|ip|resource|rule)\./$1:/gi;
        $val =~ s/(tx|ip|resource|rule)\./$1:/gi;

        # Translate field names
        $name =~ s/\%{([^}]+)}/"\%{".field_modsec2ib($1)."}"/ge;
        $val =~ s/\%{([^}]+)}/"\%{".field_modsec2ib($1)."}"/ge;

        return ($action, $name.$op.$val);
    },
    skip => sub { () },
    skipAfter => sub { () },
    status => sub { ($_[0], $_[1]) },
    t => sub {
        if ($_[1] eq "none") {
            return ();
        }
        else {
            return ($_[0], $_[1]);
        }
    },
    tag => sub { ($_[0], $_[1]) },
    ver => sub { ("tag", $_[0]."/".$_[1]) },
    xmlns => sub { () },
);

sub action_modsec2ib {
    my($name,$arg) = @_;
    if (!exists $ACTION_MAP{$name}) {
        return ();
    }

    return $ACTION_MAP{$name}($name, $arg);
}

my %OP_MAP = (
    beginsWith => sub { ("rx", "^".$_[1]) },
    contains => sub { ($_[0], $_[1]) },
    containsWord => sub { ("match", $_[1]) },
    endsWith => sub { ("rx", $_[1]."\$") },
    eq => sub { ($_[0], $_[1]) },
    ge => sub { ($_[0], $_[1]) },
    geoLookup => sub { () },
    gsbLookup => sub { () },
    gt => sub { ($_[0], $_[1]) },
    inspectFile => sub { () },
    ipMatch => sub { ("ipmatch", $_[1]) },
    ipMatchF => sub { () },
    ipMatchFromFile => sub { () },
    le => sub { ($_[0], $_[1]) },
    lt => sub { ($_[0], $_[1]) },
    nop => sub { ($_[0], $_[1]) },
    pm => sub { ("match", $_[1]) },
    pmf => sub { ($_[0], $_[1]) },
    pmFromFile => sub { ("pmf", $_[1]) },
    rbl => sub { () },
    rsub => sub { () },
    rx => sub { ($_[0], $_[1]) },
    streq => sub { ($_[0], $_[1]) },
    strmatch => sub { ("streq", $_[1]) },
    validateByteRange => sub { () },
    validateDTD => sub { () },
    validateHash => sub { ($_[0], $_[1]) },
    validateSchema => sub { ($_[0], $_[1]) },
    validateUrlEncoding => sub { ($_[0], $_[1]) },
    validateUtf8Encoding => sub { ($_[0], $_[1]) },
    verifyCC => sub { ($_[0], $_[1]) },
    verifyCPF => sub { ($_[0], $_[1]) },
    verifySSN => sub { ($_[0], $_[1]) },
    within => sub { ($_[0], $_[1]) },
);

sub op_modsec2ib {
    my($neg,$name,$arg) = @_;
    if (!exists $OP_MAP{$name}) {
        return ();
    }

    my @res = $OP_MAP{$name}($name, $arg);
    if (@res == 0) {
        return ("!", "nop", "0");
    }

    # Fix macro syntax
    $res[1] =~ s/(tx|ip|resource|rule)\./$1:/gi;
    $res[1] =~ s/\%{([^}]+)}/"\%{".field_modsec2ib($1)."}"/ge;

    return ( $neg, @res );
}

my $rparser = Parse::RecDescent->new($rule_parser);
my $full_line = "";
my $prev_rule;

# Print the header
printf(
    ("#"x78)."\n".
    "# ModSecurity to IronBee Rule Conversion\n".
    "#\n".
    "# Converted: %s\n".
    "# Script: %s\n".
    ("#"x78)."\n".
    "\n",
    scalar(localtime),
    join(" ", $0, @ARGV)
);

# Loop through all line on stdin, convert and print to stdout
while (my $line = <>) {
    my $prefix;

    # Follow line continuations
    if (!($line =~ m/^\s*#/) and ($line =~ s/\\\s*$//)) {
        $full_line .= $line;
        next;
    }
    $full_line .= $line;

    # Print empty/comment lines as-is
    if (($full_line =~ m/^\s*$/) or ($full_line =~ m/^\s*#/)) {
        print($full_line);
        $full_line = "";
        next;
    }

    # Strip whitespace
    $full_line =~ s/^(\s+)//;
    $prefix = $1;
    $full_line =~ s/\s+$//;

    # Parse other lines
    my $rule = $rparser->rule_line($full_line);
    $rule->{raw} = $full_line;
    push @{ $rule->{field}{parsed} }, grep {$_->toIBRuleLang() ne ""} @{ $rparser->fields($rule->{field}{raw}) || [] };
    $rule->{op}{parsed} = $rparser->operator($rule->{op}{raw});
    push @{ $rule->{modifier}{parsed} }, grep {$_->toIBRuleLang() ne ""} @{ $rparser->actions($rule->{modifier}{raw}) || [] };

    # Print IronBee rule prefixes with commented ModSec rule
    print($prefix."# ORIG: $full_line\n");
#    print Dumper($rule);
    print($prefix.$rule->toIBRuleLang()."\n");

    $full_line = "";
    $prev_rule = $rule;
}


