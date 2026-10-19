package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

const sample = `{
  "enums": [["color", ["RED", 0], ["BLUE", 1], {"enumtype": "u8"}]],
  "enumflags": [], "unions": [],
  "aliases": {"blob": {"type": "u8", "length": 4}},
  "types": [["pair", ["vl_api_color_t", "c"], ["u8", "default"]]],
  "messages": [
    ["thing", ["u16", "_vl_msg_id"], ["u32", "client_index"], ["u32", "context"],
     ["u8", "n"], ["vl_api_pair_t", "items", 0, "n"], ["string", "name"],
     {"crc": "0x00abcdef"}],
    ["thing_reply", ["u16", "_vl_msg_id"], ["u32", "context"], ["i32", "retval"],
     {"crc": "0x1"}]],
  "services": {"thing": {"reply": "thing_reply"}},
  "imports": []
}`

func TestGenerate(t *testing.T) {
	tmp := t.TempDir()
	in, out := filepath.Join(tmp, "in"), filepath.Join(tmp, "out")
	if err := os.MkdirAll(in, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(in, "sample.api.json"), []byte(sample), 0o644); err != nil {
		t.Fatal(err)
	}
	if n, err := generate(in, out); err != nil || n != 1 {
		t.Fatalf("generate = %d, %v", n, err)
	}
	b, err := os.ReadFile(filepath.Join(out, "sample.api.h"))
	if err != nil {
		t.Fatal(err)
	}
	text := string(b)
	for _, want := range []string{
		"enum class Color : uint8_t",
		"using Blob = std::array<uint8_t, 4>;",
		`"thing_00abcdef"`,
		`"thing_reply_00000001"`,
		"using Reply = ThingReply;",
		"std::vector<Pair> items;",
		"uint8_t default_{};",
	} {
		if !strings.Contains(text, want) {
			t.Errorf("output missing %q", want)
		}
	}
	if strings.Contains(text, "client_index") {
		t.Error("header field client_index must be implicit")
	}
}

func TestCamel(t *testing.T) {
	for in, want := range map[string]string{
		"vl_api_ip4_address_t": "Ip4Address",
		"sw_interface_details": "SwInterfaceDetails",
		"vl_api_3des_t":        "N3des",
	} {
		if got := camel(in); got != want {
			t.Errorf("camel(%q) = %q, want %q", in, got, want)
		}
	}
}

func TestUnknownLengthFieldFails(t *testing.T) {
	var out []string
	err := emitStruct("X", []field{{typ: "u8", name: "a", hasLen: true, hasLenField: true, lenField: "missing"}}, nil, "c", &out)
	if err == nil {
		t.Error("expected error")
	}
}
