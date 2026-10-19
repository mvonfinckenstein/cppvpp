// Command gen_api generates C++17 headers from VPP *.api.json files.
//
// For every <module>.api.json found below the input directory a header
// <module>.api.h is written to the output directory. Generated code lives in
// namespace vpp::api and builds on vpp/wire.h.
//
// Usage: gen_api INPUT_DIR OUTPUT_DIR
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type prim struct {
	cpp  string
	size int
}

var prims = map[string]prim{
	"u8": {"uint8_t", 1}, "i8": {"int8_t", 1},
	"u16": {"uint16_t", 2}, "i16": {"int16_t", 2},
	"u32": {"uint32_t", 4}, "i32": {"int32_t", 4},
	"u64": {"uint64_t", 8}, "i64": {"int64_t", 8},
	"f64": {"double", 8}, "bool": {"bool", 1},
}

var keywords = map[string]bool{}

func init() {
	for _, k := range strings.Fields(`alignas alignof and asm auto break case catch
		char class const continue default delete do double else enum explicit
		export extern false float for friend goto if inline int long mutable
		namespace new not operator or private protected public register return
		short signed sizeof static struct switch template this throw true try
		typedef typeid typename union unsigned using virtual void volatile
		while xor`) {
		keywords[k] = true
	}
}

// headerFields are the leading fields of every message that the Connection
// fills in itself.
var headerFields = []string{"_vl_msg_id", "client_index", "context"}

// camel converts vl_api_foo_bar_t / foo_bar to FooBar.
func camel(name string) string {
	name = strings.TrimPrefix(name, "vl_api_")
	name = strings.TrimSuffix(name, "_t")
	var b strings.Builder
	for _, p := range strings.Split(name, "_") {
		if p != "" {
			b.WriteString(strings.ToUpper(p[:1]) + p[1:])
		}
	}
	out := b.String()
	if out != "" && out[0] >= '0' && out[0] <= '9' {
		return "N" + out
	}
	return out
}

func ident(name string) string {
	if keywords[name] {
		return name + "_"
	}
	return name
}

// field is one field of a message, type or union.
type field struct {
	typ         string
	name        string
	length      int // valid if hasLen
	hasLen      bool
	lenField    string // valid if hasLenField
	hasLenField bool
}

func parseField(raw []json.RawMessage) (field, error) {
	var f field
	if len(raw) < 2 {
		return f, fmt.Errorf("short field %s", raw)
	}
	if err := json.Unmarshal(raw[0], &f.typ); err != nil {
		return f, err
	}
	if err := json.Unmarshal(raw[1], &f.name); err != nil {
		return f, err
	}
	var rest []json.RawMessage
	for _, r := range raw[2:] {
		if len(r) > 0 && r[0] != '{' {
			rest = append(rest, r)
		}
	}
	if len(rest) > 0 {
		var n int
		if json.Unmarshal(rest[0], &n) == nil {
			f.length, f.hasLen = n, true
		}
	}
	if len(rest) > 1 {
		if err := json.Unmarshal(rest[1], &f.lenField); err != nil {
			return f, err
		}
		f.hasLenField = true
	}
	return f, nil
}

func parseFields(raws []json.RawMessage) ([]field, error) {
	var out []field
	for _, r := range raws {
		var arr []json.RawMessage
		if json.Unmarshal(r, &arr) != nil {
			continue // Trailing options object.
		}
		f, err := parseField(arr)
		if err != nil {
			return nil, err
		}
		out = append(out, f)
	}
	return out, nil
}

type kind int

const (
	kEnum kind = iota
	kAlias
	kUnion
	kStruct
)

type enumItem struct{ name, value string }

// def is a type definition found in one module's JSON.
type def struct {
	kind     kind
	name     string // vl_api_*_t
	module   string
	under    string     // enum underlying type
	items    []enumItem // enum items
	aliasTyp string
	aliasLen int
	hasAlen  bool
	fields   []field // union / struct
}

type message struct {
	name   string
	fields []field
	crc    string
	module string
}

type service struct {
	Reply     string `json:"reply"`
	Stream    bool   `json:"stream"`
	StreamMsg string `json:"stream_msg"`
}

type registry struct {
	defs     map[string]*def // resolved: name -> def
	cands    []*def
	messages []message
	services map[string]map[string]service
	imports  map[string][]string
}

func newRegistry() *registry {
	return &registry{
		defs:     map[string]*def{},
		services: map[string]map[string]service{},
		imports:  map[string][]string{},
	}
}

type apiFile struct {
	Enums     [][]json.RawMessage        `json:"enums"`
	EnumFlags [][]json.RawMessage        `json:"enumflags"`
	Aliases   map[string]json.RawMessage `json:"aliases"`
	Unions    [][]json.RawMessage        `json:"unions"`
	Types     [][]json.RawMessage        `json:"types"`
	Messages  [][]json.RawMessage        `json:"messages"`
	Services  map[string]json.RawMessage `json:"services"`
	Imports   []string                   `json:"imports"`
}

func str(r json.RawMessage) string {
	var s string
	_ = json.Unmarshal(r, &s)
	return s
}

// load parses one JSON file, returning its module name and imports.
func (r *registry) load(path string) (string, []string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", nil, err
	}
	var d apiFile
	if err := json.Unmarshal(data, &d); err != nil {
		return "", nil, fmt.Errorf("%s: %v", path, err)
	}
	module := strings.TrimSuffix(filepath.Base(path), ".api.json")

	for _, e := range append(append([][]json.RawMessage{}, d.Enums...), d.EnumFlags...) {
		var opts struct {
			EnumType string `json:"enumtype"`
		}
		_ = json.Unmarshal(e[len(e)-1], &opts)
		if opts.EnumType == "" {
			opts.EnumType = "u32"
		}
		df := &def{kind: kEnum, name: "vl_api_" + str(e[0]) + "_t", module: module, under: opts.EnumType}
		for _, it := range e[1 : len(e)-1] {
			var pair []json.RawMessage
			if err := json.Unmarshal(it, &pair); err != nil || len(pair) != 2 {
				return "", nil, fmt.Errorf("%s: bad enum item %s", path, it)
			}
			df.items = append(df.items, enumItem{str(pair[0]), strings.TrimSpace(string(pair[1]))})
		}
		r.cands = append(r.cands, df)
	}
	names := make([]string, 0, len(d.Aliases))
	for k := range d.Aliases {
		names = append(names, k)
	}
	sort.Strings(names)
	for _, k := range names {
		var a struct {
			Type   string `json:"type"`
			Length *int   `json:"length"`
		}
		if err := json.Unmarshal(d.Aliases[k], &a); err != nil {
			return "", nil, err
		}
		df := &def{kind: kAlias, name: "vl_api_" + k + "_t", module: module, aliasTyp: a.Type}
		if a.Length != nil {
			df.aliasLen, df.hasAlen = *a.Length, true
		}
		r.cands = append(r.cands, df)
	}
	for _, grp := range []struct {
		k    kind
		list [][]json.RawMessage
	}{{kUnion, d.Unions}, {kStruct, d.Types}} {
		for _, u := range grp.list {
			fs, err := parseFields(u[1:])
			if err != nil {
				return "", nil, err
			}
			r.cands = append(r.cands, &def{kind: grp.k, name: "vl_api_" + str(u[0]) + "_t", module: module, fields: fs})
		}
	}
	for _, m := range d.Messages {
		var meta struct {
			CRC string `json:"crc"`
		}
		_ = json.Unmarshal(m[len(m)-1], &meta)
		fs, err := parseFields(m[1 : len(m)-1])
		if err != nil {
			return "", nil, err
		}
		r.messages = append(r.messages, message{str(m[0]), fs, meta.CRC, module})
	}
	svcs := map[string]service{}
	for k, v := range d.Services {
		var s service
		if json.Unmarshal(v, &s) == nil {
			svcs[k] = s
		}
	}
	r.services[module] = svcs
	var imports []string
	for _, i := range d.Imports {
		imports = append(imports, strings.TrimSuffix(filepath.Base(i), ".api"))
	}
	r.imports[module] = imports
	return module, imports, nil
}

func (r *registry) closure(m string, acc map[string]bool) map[string]bool {
	for _, i := range r.imports[m] {
		if !acc[i] {
			acc[i] = true
			r.closure(i, acc)
		}
	}
	return acc
}

// finalize resolves each type to the upstream-most module that defines it.
//
// VPP repeats imported definitions inside every importing module's JSON, so
// the same type shows up in many files; it must be emitted once.
func (r *registry) finalize() {
	byName := map[string][]*def{}
	for _, c := range r.cands {
		key := fmt.Sprintf("%d/%s", c.kind, c.name)
		byName[key] = append(byName[key], c)
	}
	for _, defs := range byName {
		mods := map[string]bool{}
		for _, d := range defs {
			mods[d.module] = true
		}
		var roots []*def
		for _, d := range defs {
			root := true
			for m := range r.closure(d.module, map[string]bool{}) {
				if m != d.module && mods[m] {
					root = false
				}
			}
			if root {
				roots = append(roots, d)
			}
		}
		if len(roots) == 0 {
			roots = defs
		}
		best := roots[0]
		for _, d := range roots[1:] {
			if d.module < best.module {
				best = d
			}
		}
		r.defs[best.name] = best
	}
}

func (r *registry) sizeOf(t string) int {
	if p, ok := prims[t]; ok {
		return p.size
	}
	d := r.defs[t]
	switch d.kind {
	case kEnum:
		return prims[d.under].size
	case kAlias:
		n := 1
		if d.hasAlen {
			n = d.aliasLen
		}
		return r.sizeOf(d.aliasTyp) * n
	case kUnion:
		max := 0
		for _, f := range d.fields {
			if s := r.fieldSize(f); s > max {
				max = s
			}
		}
		return max
	}
	sum := 0
	for _, f := range d.fields {
		sum += r.fieldSize(f)
	}
	return sum
}

func (r *registry) fieldSize(f field) int {
	if f.typ == "string" {
		return f.length
	}
	n := 1
	if f.hasLen && f.length != 0 {
		n = f.length
	}
	return r.sizeOf(f.typ) * n
}

// deps returns the type names directly referenced by definition d.
func deps(d *def) []string {
	if d.kind == kAlias {
		return []string{d.aliasTyp}
	}
	var out []string
	for _, f := range d.fields {
		out = append(out, f.typ)
	}
	return out
}

func cppType(t string) string {
	if p, ok := prims[t]; ok {
		return p.cpp
	}
	return camel(t)
}

// fieldCode returns the declaration, encode and decode statements of f.
func fieldCode(f field, lenFields map[string]string) (string, []string, []string) {
	n := ident(f.name)
	if f.typ == "string" {
		if f.hasLen && f.length != 0 {
			return fmt.Sprintf("std::string %s;", n),
				[]string{fmt.Sprintf("e.PutFixedString(%s, %d);", n, f.length)},
				[]string{fmt.Sprintf("%s = d.GetFixedString(%d);", n, f.length)}
		}
		return fmt.Sprintf("std::string %s;", n),
			[]string{fmt.Sprintf("e.PutVarString(%s);", n)},
			[]string{fmt.Sprintf("%s = d.GetVarString();", n)}
	}
	ct := cppType(f.typ)
	switch {
	case f.hasLenField:
		return fmt.Sprintf("std::vector<%s> %s;", ct, n),
			[]string{fmt.Sprintf("for (const auto& x : %s) WirePut(e, x);", n)},
			[]string{fmt.Sprintf("%s.resize(%s);", n, ident(f.lenField)),
				fmt.Sprintf("for (auto& x : %s) WireGet(d, &x);", n)}
	case f.hasLen && f.length == 0:
		return fmt.Sprintf("std::vector<%s> %s;  // Extends to the end of the message.", ct, n),
			[]string{fmt.Sprintf("for (const auto& x : %s) WirePut(e, x);", n)},
			[]string{fmt.Sprintf("while (d.ok() && d.remaining() > 0) { %s v{}; WireGet(d, &v); %s.push_back(v); }", ct, n)}
	case f.hasLen:
		return fmt.Sprintf("std::array<%s, %d> %s{};", ct, f.length, n),
			[]string{fmt.Sprintf("WirePut(e, %s);", n)},
			[]string{fmt.Sprintf("WireGet(d, &%s);", n)}
	}
	if arr, ok := lenFields[f.name]; ok {
		// The count is always derived from the array it describes.
		return fmt.Sprintf("%s %s{};", ct, n),
			[]string{fmt.Sprintf("WirePut(e, static_cast<%s>(%s.size()));", ct, ident(arr))},
			[]string{fmt.Sprintf("WireGet(d, &%s);", n)}
	}
	return fmt.Sprintf("%s %s{};", ct, n),
		[]string{fmt.Sprintf("WirePut(e, %s);", n)},
		[]string{fmt.Sprintf("WireGet(d, &%s);", n)}
}

func emitStruct(name string, fields []field, extra []string, comment string, out *[]string) error {
	lenFields := map[string]string{}
	names := map[string]bool{}
	for _, f := range fields {
		names[f.name] = true
		if f.hasLenField {
			lenFields[f.lenField] = f.name
		}
	}
	for lf := range lenFields {
		if !names[lf] {
			return fmt.Errorf("%s: unknown length field %s", name, lf)
		}
	}
	o := out
	add := func(s string) { *o = append(*o, s) }
	add("/// " + comment)
	add("struct " + name + " {")
	for _, x := range extra {
		add("  " + x)
	}
	var enc, dec []string
	for _, f := range fields {
		decl, e, d := fieldCode(f, lenFields)
		add("  " + decl)
		enc = append(enc, e...)
		dec = append(dec, d...)
	}
	unused := func(l []string) string {
		if len(l) == 0 {
			return "[[maybe_unused]] "
		}
		return ""
	}
	add("")
	add("  /// Appends the wire encoding of this object to @p e.")
	add("  void Encode(" + unused(enc) + "Encoder& e) const {")
	for _, x := range enc {
		add("    " + x)
	}
	add("  }")
	add("  /// Reads this object from @p d; check d.ok() afterwards.")
	add("  void Decode(" + unused(dec) + "Decoder& d) {")
	for _, x := range dec {
		add("    " + x)
	}
	add("  }")
	add("};")
	add("")
	return nil
}

func (r *registry) emitUnion(d *def, out *[]string) {
	add := func(s string) { *out = append(*out, s) }
	add(fmt.Sprintf("/// Union %s; the active member is chosen by a sibling discriminator, so it is kept as raw wire bytes.", d.name))
	add("struct " + camel(d.name) + " {")
	add(fmt.Sprintf("  std::array<uint8_t, %d> raw{};", r.sizeOf(d.name)))
	add("  /// Appends the raw bytes to @p e.")
	add("  void Encode(Encoder& e) const { WirePut(e, raw); }")
	add("  /// Reads the raw bytes from @p d.")
	add("  void Decode(Decoder& d) { WireGet(d, &raw); }")
	add("};")
	add("")
}

func emitEnum(d *def, out *[]string) {
	add := func(s string) { *out = append(*out, s) }
	cname := camel(d.name)
	ut := prims[d.under].cpp
	add("/// Enumeration " + d.name + ".")
	add(fmt.Sprintf("enum class %s : %s {", cname, ut))
	for _, it := range d.items {
		add(fmt.Sprintf("  %s = %s,", it.name, it.value))
	}
	add("};")
	add("/// Appends enum @p v to @p e.")
	add(fmt.Sprintf("inline void WirePut(Encoder& e, %s v) { WirePut(e, static_cast<%s>(v)); }", cname, ut))
	add("/// Reads enum @p v from @p d.")
	add(fmt.Sprintf("inline void WireGet(Decoder& d, %s* v) { %s x{}; WireGet(d, &x); *v = static_cast<%s>(x); }", cname, ut, cname))
	add("")
}

func emitAlias(d *def, out *[]string) {
	base := cppType(d.aliasTyp)
	if d.hasAlen {
		base = fmt.Sprintf("std::array<%s, %d>", base, d.aliasLen)
	}
	*out = append(*out, "/// Alias "+d.name+".", fmt.Sprintf("using %s = %s;", camel(d.name), base), "")
}

// moduleDefs returns the definitions of a module in dependency order.
func (r *registry) moduleDefs(module string) []*def {
	var names []string
	for n, d := range r.defs {
		if d.module == module {
			names = append(names, n)
		}
	}
	sort.Strings(names)
	inModule := map[string]bool{}
	for _, n := range names {
		inModule[n] = true
	}
	var ordered []*def
	seen := map[string]bool{}
	var visit func(n string)
	visit = func(n string) {
		if seen[n] {
			return
		}
		seen[n] = true
		for _, dep := range deps(r.defs[n]) {
			if inModule[dep] {
				visit(dep)
			}
		}
		ordered = append(ordered, r.defs[n])
	}
	for _, n := range names {
		visit(n)
	}
	return ordered
}

var nonWord = regexp.MustCompile(`\W`)

func (r *registry) generateModule(module string, imports []string, outDir string) error {
	var out []string
	add := func(s string) { out = append(out, s) }
	guard := "CPPVPP_VPP_API_" + strings.ToUpper(nonWord.ReplaceAllString(module, "_")) + "_API_H_"
	add(fmt.Sprintf("// Generated by tools/gen_api from %s.api.json. DO NOT EDIT.", module))
	add("#ifndef " + guard)
	add("#define " + guard)
	add("")
	add("#include <array>\n#include <cstdint>\n#include <string>\n#include <vector>\n")
	add("#include \"vpp/wire.h\"")
	for _, imp := range imports {
		add(fmt.Sprintf("#include \"vpp/api/%s.api.h\"", imp))
	}
	add("")
	add(fmt.Sprintf("/// \\file\n/// Types and messages of the VPP \"%s\" API module.", module))
	add("namespace vpp {\nnamespace api {\n")
	add("using vpp::WireGet;\nusing vpp::WirePut;\n")

	for _, d := range r.moduleDefs(module) {
		switch d.kind {
		case kEnum:
			emitEnum(d, &out)
		case kAlias:
			emitAlias(d, &out)
		case kUnion:
			r.emitUnion(d, &out)
		default:
			if err := emitStruct(camel(d.name), d.fields, nil, "Wire type "+d.name+".", &out); err != nil {
				return err
			}
		}
	}

	services := r.services[module]
	for _, m := range r.messages {
		if m.module == module {
			add("struct " + camel(m.name) + ";")
		}
	}
	add("")
	for _, m := range r.messages {
		if m.module != module {
			continue
		}
		body := m.fields
		for _, h := range headerFields {
			if len(body) > 0 && body[0].name == h {
				body = body[1:]
			}
		}
		crc, err := strconv.ParseUint(strings.TrimPrefix(m.crc, "0x"), 16, 32)
		if err != nil {
			return fmt.Errorf("message %s: bad crc %q", m.name, m.crc)
		}
		extra := []string{
			"/// Message name including CRC, as known to VPP.",
			fmt.Sprintf("static constexpr const char* kName = \"%s_%08x\";", m.name, crc),
		}
		if svc, ok := services[m.name]; ok && svc.Reply != "" && svc.Reply != "null" {
			if svc.Stream {
				msg := svc.StreamMsg
				if msg == "" {
					msg = svc.Reply
				}
				extra = append(extra, "/// Per-item details message type.", "using Details = "+camel(msg)+";")
			} else {
				extra = append(extra, "/// Reply message type.", "using Reply = "+camel(svc.Reply)+";")
			}
		}
		if err := emitStruct(camel(m.name), body, extra, "API message "+m.name+".", &out); err != nil {
			return err
		}
	}

	add("}  // namespace api\n}  // namespace vpp\n")
	add("#endif  // " + guard)
	return os.WriteFile(filepath.Join(outDir, module+".api.h"), []byte(strings.Join(out, "\n")+"\n"), 0o644)
}

// generate converts every *.api.json below inDir into a header in outDir.
func generate(inDir, outDir string) (int, error) {
	if err := os.MkdirAll(outDir, 0o755); err != nil {
		return 0, err
	}
	var paths []string
	err := filepath.Walk(inDir, func(p string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() && strings.HasSuffix(p, ".api.json") {
			paths = append(paths, p)
		}
		return nil
	})
	if err != nil {
		return 0, err
	}
	sort.Strings(paths)
	reg := newRegistry()
	modules := map[string][]string{}
	var order []string
	for _, p := range paths {
		m, imps, err := reg.load(p)
		if err != nil {
			return 0, err
		}
		if _, dup := modules[m]; !dup {
			order = append(order, m)
		}
		modules[m] = imps
	}
	reg.finalize()
	seen := map[string]string{}
	for n := range reg.defs {
		seen[camel(n)] = n
	}
	for _, m := range reg.messages {
		if other, ok := seen[camel(m.name)]; ok {
			return 0, fmt.Errorf("name collision: message %s vs %s", m.name, other)
		}
	}
	for _, m := range order {
		if err := reg.generateModule(m, modules[m], outDir); err != nil {
			return 0, err
		}
	}
	return len(modules), nil
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: gen_api INPUT_DIR OUTPUT_DIR")
		os.Exit(2)
	}
	n, err := generate(os.Args[1], os.Args[2])
	if err != nil {
		fmt.Fprintln(os.Stderr, "gen_api:", err)
		os.Exit(1)
	}
	fmt.Printf("generated %d headers in %s\n", n, os.Args[2])
}
