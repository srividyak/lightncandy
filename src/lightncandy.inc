<?php
/* 

Copyrights for code authored by Yahoo! Inc. is licensed under the following terms:
MIT License
Copyright (c) 2013 Yahoo! Inc. All Rights Reserved.
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Origin: https://github.com/zordius/lightncandy 
*/

/**
 * This is abstract engine which defines must-have methods.
 *
 * @package    LightnCandy
 * @subpackage Core
 * @author     Zordius <zordius@yahoo-inc.com>
 */

/**
 * LightnCandy static core class.
 */
class LightnCandy {
    const FLAG_ERROR_LOG = 1;
    const FLAG_ERROR_EXCEPTION = 2;
    const FLAG_STANDALONE = 4;
    const FLAG_JSTRUE = 8;
    const FLAG_JSOBJECT = 16;
    const FLAG_THIS = 32;
    const FLAG_WITH = 64;
    const FLAG_PARENT = 128;

    const FLAG_BESTPERFORMANCE = 0;
    const FLAG_HANDLEBARSJS = 248; // FLAG_JSTRUE + FLAG_JSOBJECT + FLAG_THIS + FLAG_WITH + FLAG_PARENT

    const PARTIAL_SEARCH = '/\\{\\{>[ \\t]*(.+?)[ \\t]*\\}\\}/s';
    const TOKEN_SEARCH = '/(\\{{2,3})(.+?)(\\}{2,3})/s';

    private static $lastContext;

    /**
     * Compile handlebars template into PHP code
     *
     * @param string $template handlebars template string
     * @param array $options LightnCandy compile time and run time options, default is Array('flags' => LightnCandy::FLAG_BESTPERFORMANCE)
     *
     * @return string Compiled PHP code when successed. If error happened and compile failed, return false.
     */
    public static function compile($template, $options = 0) {
        return self::compiledCode($template, $options);
    }

    /**
     * Internal method used by compile(). Get compiled code from a template string
     *
     * @param string $template handlebars template string
     * @param array $options LightnCandy compile time and run time options, default is Array('flags' => LightnCandy::FLAG_BESTPERFORMANCE)
     *
     * @return string Compiled PHP code when successed. If error happened and compile failed, return false.
     */
    protected static function compiledCode($template, $options) {
        $flags = isset($options['flags']) ? $options['flags'] : self::FLAG_BESTPERFORMANCE;

        $context = Array(
            'flags' => Array(
                'errorlog' => $flags & self::FLAG_ERROR_LOG,
                'exception' => $flags & self::FLAG_ERROR_EXCEPTION,
                'standalone' => $flags & self::FLAG_STANDALONE,
                'jstrue' => $flags & self::FLAG_JSTRUE,
                'jsobj' => $flags & self::FLAG_JSOBJECT,
                'this' => $flags & self::FLAG_THIS,
                'with' => $flags & self::FLAG_WITH,
                'parent' => $flags & self::FLAG_PARENT,
            ),
            'level' => 0,
            'stack' => Array(),
            'error' => false,
            'useVar' => false,
            'vars' => Array(),
            'jsonSchema' => Array(
                '$schema' => 'http://json-schema.org/draft-03/schema',
                'description' => 'Template Json Schema'
            ),
            'basedir' => self::_basedir($options),
            'fileext' => self::_fileext($options),
            'usedPartial' => Array(),
            'usedFeature' => Array(
                'rootvar' => 0,
                'rootthis' => 0,
                'enc' => 0,
                'raw' => 0,
                'sec' => 0,
                'isec' => 0,
                'if' => 0,
                'else' => 0,
                'unless' => 0,
                'each' => 0,
                'this' => 0,
                'parent' => 0,
                'with' => 0,
                'dot' => 0,
                'comment' => 0,
                'partial' => 0,
            )
        );

        // Scan for partial and replace partial with template
        $template = preg_replace_callback(self::PARTIAL_SEARCH, function ($matches) use (&$context) {
            return LightnCandy::readPartial($matches[1], $context);
        }, $template);

        if (self::_error($context)) {
            return false;
        }

        // Do first time scan to find out used feature, detect template error.
        if (preg_match_all(self::TOKEN_SEARCH, $template, $tokens, PREG_SET_ORDER) > 0) {
            foreach ($tokens as $token) {
                self::scan($token, $context);
            }
        }

        if (self::_error($context)) {
            return false;
        }

        // Check used features and compile flags. If the template is simple enough,
        // we can generate best performance code with enable 'useVar' internal flag.
        if (!$context['flags']['jsobj'] && (($context['usedFeature']['sec'] + $context['usedFeature']['parent'] < 1) || !$context['flags']['jsobj'])) {
            $context['useVar'] = Array('$in');
        }

        // Do PHP code and json schema generation.
        $code = preg_replace_callback(self::TOKEN_SEARCH, function ($matches) use (&$context) {
            return '\'' . LightnCandy::tokens($matches, $context) . '\'';
        }, addcslashes($template, "'"));

        if (self::_error($context)) {
            return false;
        }

        $flagJStrue = self::_on($context['flags']['jstrue']);
        $flagJSObj = self::_on($context['flags']['jsobj']);

        $libstr = self::exportLCRun($context);

        // return generated PHP code string
        return "<?php return function (\$in) {
    \$cx = Array(
        'flags' => Array(
            'jstrue' => $flagJStrue,
            'jsobj' => $flagJSObj,
        ),
        'scopes' => Array(),
        'path' => Array(),
        'parents' => Array(),
$libstr
    );
    return '$code';
}
?>";
    }

    /**
     * read partial file content as string
     *
     * @param string $name partial file name
     * @param array $context Current context of compiler progress.
     *
     * @return string partial file content
     */
    public static function readPartial($name, &$context) {
        $context['usedFeature']['partial'] ++;
        foreach ($context['basedir'] as $dir) {
            foreach ($context['fileext'] as $ext) {
                $fn = "$dir/$name$ext";
                if (file_exists($fn)) {
                    return file_get_contents($fn);
                }
            }
        }
        $context['error'] = "can not find partial file for '$name', you should set correct basedir and fileext in options";
    }

    /**
     * Internal method used by compile(). check options and handle fileext 
     *
     * @param string $options current compile option
     *
     * @return array file extensions
     */
    protected static function _fileext($options) {
        $exts = isset($options['fileext']) ? $options['fileext'] : '.tmpl';
        return is_array($exts) ? $exts : Array($exts);
    }

    /**
     * Internal method used by compile(). check options and handle basedir
     *
     * @param string $options current compile option
     *
     * @return array base directories
     */
    protected static function _basedir($options) {
        $dirs = isset($options['basedir']) ? $options['basedir'] : 0;
        $dirs = is_array($dirs) ? $dirs : Array($dirs);

        foreach ($dirs as $index => $dir) {
            if (!is_dir($dir)) {
                unset($dirs[$index]);
            }
        }

        if (count(array_keys($dirs)) === 0) {
            $dirs[] = getcwd();
        }

        return $dirs;
    }

    /**
     * Internal method used by compile(). export required standalone functions.
     *
     * @param string $context current scaning context
     */
    protected static function exportLCRun($context) {
        if ($context['flags']['standalone'] == 0) {
            return '';
        }

        $class = new ReflectionClass('LCRun');
        $fname = $class->getFileName();
        $lines = file_get_contents($fname);
        $file = new SplFileObject($fname);
        $ret = "'funcs' => Array(\n";

        foreach ($class->getMethods() as $method) {
            $file->seek($method->getStartLine() - 2);
            $spos = $file->ftell();
            $file->seek($method->getEndLine() - 2);
            $epos = $file->ftell();
            $ret .= preg_replace('/self::(.+)\(/', '\\$cx[\'funcs\'][\'$1\'](', preg_replace('/public static function (.+)\\(/', '\'$1\' => function (', substr($lines, $spos, $epos - $spos))) . "    },\n";
        }
        unset($file);
        return "$ret)\n";
    }

    /**
     * Internal method used by compile().
     *
     * @param array $context Current context of compiler progress.
     *
     * @return boolean True when error detected
     */
    protected static function _error($context) {
        self::$lastContext = $context;
        if ($context['error']) {
            if ($context['flags']['errorlog']) {
                error_log($context['error']);
            }
            if ($context['flags']['exception']) {
                throw new Exception($context['error']);
            }
            return true;
        }
        return false;
    }

    /**
     * Internal method used by compile().
     *
     * @param mixed $v value
     *
     * @return boolean True when the value larger then 0
     */
    protected static function _on($v) {
        return ($v > 0) ? 'true' : 'false';
    }

    /**
     * get last compiler context.
     *
     * @return array Context data
     */
    public static function getContext() {
        return self::$lastContext;
    }

    /**
     * get JsonSchema of last compiled handlebars template.
     *
     * @return array JsonSchema data
     */
    public static function getJsonSchema() {
        return self::$lastContext['jsonSchema'];
    }

    /**
     * get JsonSchema of last compiled handlebars template as pretty printed string.
     *
     * @return string JsonSchema string
     */
    public static function getJsonSchemaString($indent = '  ') {
        $level = 0;
        return preg_replace_callback('/\\{|\\[|,|\\]|\\}|:/', function ($matches) use (&$level) {
            switch ($matches[0]) {
                case '}':
                case ']':
                    $level--;
                    $is = str_repeat('  ', $level);
                    return "\n$is{$matches[0]}";
                case ':':
                    return ': ';
            }
            $br = '';
            switch ($matches[0]) {
                case '{':
                case '[':
                    $level ++;
                case ',':
                    $br = "\n";
            }
            $is = str_repeat('  ', $level);
            return "{$matches[0]}$br$is";
        }, json_encode(self::getJsonSchema()));
    }

    /**
     * include a string as php codes. this method requires php setting allow_url_include=1 and allow_url_fopen=1
     *
     * @param string $php php codes
     * @param string $tmp_dir optional, change temp directory for php include file saved by prepare() when can not include php code with data:// format.
     *
     * @return mixed result of include()
     */
    public static function prepare($php, $tmp_dir = false) {
        if (!ini_get('allow_url_include') || !ini_get('allow_url_fopen')) {
            if (!is_dir($tmp_dir)) {
                $tmp_dir = sys_get_temp_dir();
            }
        }

        if ($tmp_dir) {
            $fn = tempnam($tmp_dir, 'lci_');
            if (!$fn) {
                die("Can not generate tmp file under $tmp_dir!!\n");
            }
            if (!file_put_contents($fn, $php)) {
                die("Can not include saved temp php code from $fn, you should add $tmp_dir into open_basedir!!\n");
            }
            return include($fn);
        }

        return include('data://text/plain,' . urlencode($php));
    }

    /**
     * include a compiled and saved template php file, then render with input data.
     *
     * @param string $compiled compiled template php file name
     *
     * @return string rendered result
     */
    public static function render($compiled, $data) {
        $func = include($compiled);
        return $func($data);
    }

    /**
     * Internal method used by compile(). Get function name for standalone or none standalone tempalte.
     *
     * @param array $context Current context of compiler progress.
     * @param string $name base function name
     *
     * @return string compiled Function name
     */
    protected static function _fn($context, $name) {
        return $context['flags']['standalone'] ? "\$cx['funcs']['$name']" : "LCRun::$name";
    }

    /**
     * Internal method used by compile(). Get variable names translated array, Ex: a.b.c => ["'a'", "'b'", "'c'"]
     *
     * @param array $scopes an array of variable names with single quote
     *
     * @return string PHP array names string
     */
    protected static function _scope($scopes) {
        return count($scopes) ? '[' . implode('][', $scopes) . ']' : '';
    }

    /**
     * Internal method used by compile(). Get variable names translated string, Ex: a.b.c => "['a']['b']['c']"
     *
     * @param array $scopes an array of variable names. ex: ['a', 'b', 'c', ...]
     *
     * @return string Translated variable name as input array notation.
     */
    protected static function _qscope($list) {
        return self::_scope(array_map(function ($v) {return "'$v'";}, $list));
    }

    /**
     * Internal method used by compile(). Get variable names translated string, Ex: a.b.c => "['a']['b']['c']"
     *
     * @param string $vn variable name.
     *
     * @return string Translated variable name as input array notation.
     */
    protected static function _vn($vn) {
        return $vn ? self::_qscope(explode('.', $vn)) : '';
    }

    /**
     * Internal method used by compile().
     *
     * @param mixed $v variable name to be fixed.
     * @param array $context Current compile content.
     */
    protected static function _vx(&$v, $context) {
        $v = trim($v);
        if (($v == 'this') || $v == '.') {
            if ($context['flags']['this']) {
                $v = null;
            }
        }
    }

    /**
     * Internal method used by compile().
     *
     * @param string $v variable name.
     *
     * @return mixed Variable names array or null.
     */
    protected static function _vs($v) {
        if ($v == '.') {
            return Array('.');
        }
        return $v ? explode('.', $v) : null;
    }

    /**
     * Internal method used by compile(). Find current json schema target, return childrens.
     *
     * @param array $target current json schema target
     * @param mixed $key move target to child specified with the key
     *
     * @return array children of new json schema target 
     */
    protected static function &_jst(&$target, $key = false) {
        if ($key) {
            if (!isset($target['properties'])) {
                $target['type'] = 'object';
                $target['properties'] = Array();
            }
            if (!isset($target['properties'][$key])) {
                $target['properties'][$key] = Array();
            }
            return $target['properties'][$key];
        } else {
            if (!isset($target['items'])) {
                $target['type'] = 'array';
                $target['items'] = Array();
            }
            return $target['items'];
        }
    }

    /**
     * Internal method used by compile(). Find current json schema target, prepare target parent.
     *
     * @param array $context current compile context
     * @param string $var current variable name
     */
    protected static function &_jsp(&$context) {
        $target = &$context['jsonSchema'];
        foreach ($context['vars'] as $var) {
            if ($var) {
                foreach ($var as $v) {
                    $target = &self::_jst($target, $v);
                }
            }
            $target = &self::_jst($target);
        }
        return $target;
    }

    /**
     * Internal method used by compile(). Define a json schema string/number with the variable name.
     *
     * @param array $context current compile context
     * @param string $var current variable name
     */
    protected static function _jsv(&$context, $var) {
        $target = &self::_jsp($context);
        foreach (self::_vs($var) as $v) {
            $target = &self::_jst($target, $v);
        }
        $target['type'] = Array('string', 'number');
        $target['required'] = true;
    }

    /**
     * Internal method used by compile(). Collect handlebars usage information, detect template error.
     *
     * @param string $token detected handlebars {{ }} token
     * @param string $context current scaning context
     */
    protected static function scan($token, &$context) {
        $head = substr($token[2], 0, 1);
        $act = substr($token[2], 1);
        $raw = ($token[1] === '{{{');

        if (count($token[1]) !== count($token[3])) {
            $context['error'] = "Bad token {$token[1]}{$token[2]}{$token[3]} ! Do you mean {{}} or {{{}}}?";
            return;
        }

        if ($raw) {
            if (preg_match('/\\^|\\/|#/', $head)) {
                $context['error'] = "Bad token {$token[1]}{$token[2]}{$token[3]} ! Do you mean \{\{{$token[2]}\}\}?";
                return;
            }
        }

        switch ($head) {
        case '^':
            return $context['usedFeature']['isec'] ++;

            case '/':
            $context['level']--;
            return;

        case '#':
            $acts = explode(' ', $act);
            switch ($acts[0]) {
            case 'with':
                if (isset($acts[1]) && !$context['flags']['with']) {
                    $context['error'] = 'do not support {{#with var}}, you should do compile with LightnCandy::FLAG_WITH flag';
                }
            case 'each':
                $context['level'] ++;
            case 'unless':
            case 'if':
                return $context['usedFeature'][$acts[0]] ++;

            default:
                $context['level'] ++;
                return $context['usedFeature']['sec'] ++;
            }

        case '!':
            return $context['usedFeature']['comment'] ++;

        default:
            $fn = $raw ? 'raw' : 'enc';
            $context['usedFeature'][$fn] ++;
            $token[2] = trim($token[2]);
            switch ($token[2]) {
                case 'else':
                    return $context['usedFeature']['else'] ++;

                case 'this':
                    if ($context['level'] == 0) {
                        $context['usedFeature']['rootthis'] ++;
                    }
                    if (!$context['flags']['this']) {
                        $context['error'] = 'do not support {{this}}, you should do compile with LightnCandy::FLAG_THIS flag';
                    }
                    return $context['usedFeature']['this'] ++;

                case '.':
                    if ($context['level'] == 0) {
                        $context['usedFeature']['rootthis'] ++;
                    }
                    if (!$context['flags']['this']) {
                        $context['error'] = 'do not support {{.}}, you should do compile with LightnCandy::FLAG_THIS flag';
                    }
                    return $context['usedFeature']['dot'] ++;

                default:
                    if (preg_match('/\\.\\.(\\/.+)*/', $token[2])) {
                        if (!$context['flags']['parent']) {
                            $context['error'] = 'do not support {{../var}}, you should do compile with LightnCandy::FLAG_PARENT flag';
                        }
                        return $context['usedFeature']['parent'] ++;
                    }
            }
            if ($context['level'] == 0) {
                $context['usedFeature']['rootvar'] ++;
            }
        }
    }

    /**
     * Internal method used by compile(). Return compiled PHP code partial for a handlebars token.
     *
     * @param string $token detected handlebars {{ }} token
     * @param string $context current scaning context
     */
    public static function tokens($token, &$context) {
        $head = substr($token[2], 0, 1);
        $act = substr($token[2], 1);
        $raw = ($token[1] === '{{{');

        switch ($head) {
        case '^':
            $context['stack'][] = $act;
            $context['stack'][] = '^';
            if ($context['useVar']) {
                $v = end($context['useVar']) . "['{$act}']";
                return ".((is_null($v) && ($v !== false)) ? ("; 
            } else {
                return '.(' . self::_fn($context, 'isec') . "('$act', \$cx, \$in) ? (";
            }
        case '/':
            $each = false;
            switch ($act) {
            case 'if':
            case 'unless':
                $pop = array_pop($context['stack']);
                if ($pop == ':') {
                    $pop = array_pop($context['stack']);
                    return $context['usedFeature']['parent'] ? ';}).' : ')).';
                }
                return $context['usedFeature']['parent'] ? ';}).' : ') : \'\').';
            case 'with':
                $pop = array_pop($context['stack']);
                if ($pop !== 'with') {
                   $context['error'] = 'Unexpect token /with !';
                   return;
                }
                return ';}).';
            case 'each':
                $each = true;
            default:
                self::_vx($act, $context);
                $context['level']--;
                array_pop($context['vars']);
                $pop = array_pop($context['stack']);
                switch($pop) {
                case '#':
                case '^':
                    $pop2 = array_pop($context['stack']);
                    if (!$each && ($pop2 !== $act)) {
                        $context['error'] = "Unexpect token {$token[2]} ! Previous token $pop$pop2 is not closed";
                        return;
                    }
                    if ($pop == '^') {
                        return $context['usedFeature']['parent'] ? ';}).' : ') : \'\').';
                    }
                    return ';}).';
                default:
                    $context['error'] = "Unexpect token: {$token[2]} !";
                    return;
                }
            }
        case '#':
            $each = 'false';
            $acts = explode(' ', $act);
            switch ($acts[0]) {
            case 'if':
                $context['stack'][] = 'if';
                self::_vx($acts[1], $context);
                return $context['usedFeature']['parent'] 
                       ? '.' . self::_fn($context, 'ifv') . "('{$acts[1]}', \$cx, \$in, function(\$cx, \$in) {return "
                       : '.(' . self::_fn($context, 'ifvar') . "('{$acts[1]}', \$cx, \$in) ? (";
            case 'unless':
                $context['stack'][] = 'unless';
                self::_vx($acts[1], $context);
                return $context['usedFeature']['parent']
                       ? '.' . self::_fn($context, 'unl') . "('{$acts[1]}', \$cx, \$in, function(\$cx, \$in) {return "
                       : '.(!' . self::_fn($context, 'ifvar') . "('{$acts[1]}', \$cx, \$in) ? (";
            case 'each':
                $each = 'true';
            case 'with':
                $act = $acts[1];
            default:
                self::_vx($act, $context);
                $context['level'] ++;
                $context['vars'][] = self::_vs($act);
                if (($acts[0] === 'with') && $context['flags']['with']) {
                    $context['stack'][] = 'with';
                    return '.' . self::_fn($context, 'wi') . "('{$acts[1]}', \$cx, \$in, function(\$cx, \$in) {return ";
                }
                self::_jsp($context);
                $context['stack'][] = $act;
                $context['stack'][] = '#';
                return '.' . self::_fn($context, 'sec') . "('$act', \$cx, \$in, $each, function(\$cx, \$in) {return ";
            }
        case '!':
            return '.';
        default:
            self::_vx($token[2], $context);
            if ($token[2] ==='else') {
                $context['stack'][] = ':';
                return $context['usedFeature']['parent'] ? ';}, function($cx, $in) {return ' : ') : (';
            }
            self::_jsv($context, $token[2]);
            $fn = $raw ? 'raw' : 'enc';
            if ($context['useVar']) {
                $v = end($context['useVar']) . self::_vn($token[2]);
                if ($context['flags']['jstrue']) {
                    return $raw ? ".(($v === true) ? 'true' : $v)." : ".(($v === true) ? 'true' : htmlentities($v, ENT_QUOTES)).";
                } else {
                    return $raw ? ".$v." : ".htmlentities($v, ENT_QUOTES).";
                }
            } else {
                return '.' . self::_fn($context, $fn) . "('{$token[2]}', \$cx, \$in).";
            }
        }
    }
}

/**
 * LightnCandy static class for compiled template runtime methods.
 */
class LCRun {
    /**
     * LightnCandy runtime method for {{#if var}}
     *
     * @param string $var variable name to be tested
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return boolean Return true when the value is not null nor false.
     */
    public static function ifvar($var, $cx, $in) {
        $v = self::val($var, $cx, $in);
        return !is_null($v) && ($v !== false) && ($v !== 0);
    }

    /**
     * LightnCandy runtime method for {{#if var}} when {{../var}} used
     *
     * @param string $var variable name to be tested
     * @param array $cx render time context
     * @param array $in input data with current scope
     * @param function $truecb callback function when test result is true
     * @param function $falsecb callback function when test result is false
     *
     * @return string The rendered string of the section
     */
    public static function ifv($var, $cx, $in, $truecb, $falsecb = null) {
        $v = self::val($var, $cx, $in);
        $ret = '';
        if (is_null($v) || ($v === false)) {
            if ($falsecb) {
                $cx['scopes'][] = $in;
                $ret = $falsecb($cx, $in);
                array_pop($cx['scopes']);
            }
        } else {
            if ($truecb) {
                $cx['scopes'][] = $in;
                $ret = $truecb($cx, $in);
                array_pop($cx['scopes']);
            }
        }
        return $ret;
    }

    /**
     * LightnCandy runtime method for {{$unless var}} when {{../var}} used
     *
     * @param string $var variable name to be tested
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return boolean Return true when the value is not null nor false.
     */
    public static function unl($var, $cx, $in, $truecb, $falsecb = null) {
        return self::ifv($var, $cx, $in, $falsecb, $truecb);
    }

    /**
     * LightnCandy runtime method for {{^var}} inverted section
     *
     * @param string $var variable name to be tested
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return boolean Return true when the value is not null nor false.
     */
    public static function isec($var, $cx, $in) {
        return !self::ifvar($var, $cx, $in);
    }

    /**
     * LightnCandy runtime method to get input value
     *
     * @param string $var variable name to get the raw value
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return mixed The raw value of the specified variable
     */
    public static function val($var, $cx, $in) {
        $levels = 0;
        $var = preg_replace_callback('/\\.\\.\\//', function($matches) use (&$levels) {
            $levels++;
            return '';
        }, $var);
        if ($levels > 0) {
            $pos = count($cx['scopes']) - $levels;
            if ($pos >= 0) {
                $in = $cx['scopes'][$pos];
            } else {
                return '';
            }
        }
        if (preg_match('/(.+?)\\.(.+)/', $var, $matched)) {
            if (array_key_exists($matched[1], $in)) {
                return self::val($matched[2], $cx, $in[$matched[1]]);
            } else {
                return null;
            }
        }
        return ($var === '') ? $in : (is_array($in) ? $in[$var] : null);
    }

    /**
     * LightnCandy runtime method for {{{var}}}
     *
     * @param string $var variable name to get the raw value
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return mixed The raw value of the specified variable
     */
    public static function raw($var, $cx, $in) {
        $v = self::val($var, $cx, $in);
        if ($v === true) {
            if ($cx['flags']['jstrue']) {
                return 'true';
            }
        } elseif (is_array($v)) {
            if ($cx['flags']['jsobj']) {
                if (count(array_diff_key($v, array_keys(array_keys($v)))) > 0) {
                    return '[object Object]';
                } else {
                    $ret = Array();
                    foreach ($v as $k => $vv) {
                        $ret[] = self::raw($k, $cx, $v);
                    }
                    return join(',', $ret);
                }
            }
        }
        return $v;
    }

    /**
     * LightnCandy runtime method for {{var}}
     *
     * @param string $var variable name to get the htmlencoded value
     * @param array $cx render time context
     * @param array $in input data with current scope
     *
     * @return mixed The htmlencoded value of the specified variable
     */
    public static function enc($var, $cx, $in) {
        return htmlentities(self::raw($var, $cx, $in), ENT_QUOTES);
    }

    /**
     * LightnCandy runtime method for {{#var}} section
     *
     * @param string $var variable name for section
     * @param array $cx render time context
     * @param array $in input data with current scope
     * @param boolean $each true when rendering #each
     * @param function $cb callback function to render child context
     *
     * @return string The rendered string of the section
     */
    public static function sec($var, &$cx, $in, $each, $cb) {
        $v = self::val($var, $cx, $in);
        $loop = $each;
        if (!$loop && is_array($v)) {
            $loop = (count(array_diff_key($v, array_keys(array_keys($v)))) == 0);
        }
        if ($loop) {
            $ret = Array();
            $cx['scopes'][] = $v;
            foreach ($v as $raw) {
                $ret[] = $cb($cx, $raw);
            }
            array_pop($cx['scopes']);
            return join('', $ret);
        }
        if ($each) {
            return '';
        }
        if (is_array($v)) {
            $cx['scopes'][] = $v;
            $ret = $cb($cx, $v);
            array_pop($cx['scopes']);
            return $ret;
        }
        if ($v === true) {
            return $cb($cx, $in);
        }
        if (is_string($v)) {
            return $cb($cx, Array());
        }
        if (!is_null($v) && ($v !== false)) {
            return $cb($cx, $v);
        }
        return '';
    }

    /**
     * LightnCandy runtime method for {{#with var}}
     *
     * @param string $var variable name for section
     * @param array $cx render time context
     * @param array $in input data with current scope
     * @param function $cb callback function to render child context
     *
     * @return string The rendered string of the token
     */
    public static function wi($var, &$cx, $in, $cb) {
        $v = self::val($var, $cx, $in);
        if (($v === false) || ($v === null)) {
            return '';
        }
        $cx['scopes'][] = $in;
        $ret = $cb($cx, $v);
        array_pop($cx['scopes']);
        return $ret;
    }
}
?>
