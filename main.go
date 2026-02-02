//go:build windows

package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"sync"
	"time"

	"github.com/Microsoft/go-winio"
)

const (
	pipeName      = `\\.\pipe\bang_log`
	dumpEvery     = 5 * time.Second
	outPath       = `C:\ProgramData\bang\pipe_dump.log`
	maxBufferSize = 10 * 1024 * 1024 // 10MB safety cap (optional)
)

type spool struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (s *spool) append(p []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()

	// Optional safety cap: if the buffer is huge, drop old content
	if s.buf.Len()+len(p) > maxBufferSize {
		// naive policy: clear buffer if it would exceed cap
		s.buf.Reset()
	}
	s.buf.Write(p)
}

func (s *spool) dumpToFile(path string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.buf.Len() == 0 {
		return nil
	}

	// Ensure directory exists
	if err := os.MkdirAll(dirOf(path), 0755); err != nil {
		return err
	}

	f, err := os.OpenFile(path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
	if err != nil {
		return err
	}
	defer f.Close()

	if _, err := f.Write(s.buf.Bytes()); err != nil {
		return err
	}

	// Optional: add a separator between dumps
	if _, err := f.WriteString("\r\n"); err != nil {
		return err
	}

	s.buf.Reset()
	return nil
}

func dirOf(p string) string {
	// tiny helper so we don’t pull in filepath for one call if you don’t want to;
	// but filepath is fine too. Here’s the “proper” way:
	// return filepath.Dir(p)
	for i := len(p) - 1; i >= 0; i-- {
		if p[i] == '\\' || p[i] == '/' {
			return p[:i]
		}
	}
	return "."
}

func main() {
	fmt.Println("Starting named pipe server:", pipeName)

	var sp spool

	// dumper goroutine
	go func() {
		t := time.NewTicker(dumpEvery)
		defer t.Stop()

		for range t.C {
			if err := sp.dumpToFile(outPath); err != nil {
				fmt.Println("dump error:", err)
			}
		}
	}()

	// Main accept loop: go-winio listener accepts connections like net.Listener
	ln, err := winio.ListenPipe(pipeName, &winio.PipeConfig{
		SecurityDescriptor: "D:P(A;;GA;;;WD)", // Everyone: Full control (demo)
		MessageMode:        false,             // byte-stream mode (matches your C++ WriteFile)
		InputBufferSize:    64 * 1024,
		OutputBufferSize:   64 * 1024,
	})
	if err != nil {
		fmt.Println("ListenPipe error:", err)
		os.Exit(1)
	}
	defer ln.Close()

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println("Accept error:", err)
			time.Sleep(250 * time.Millisecond)
			continue
		}

		// Handle each client concurrently
		go handleConn(conn, &sp)
	}
}

func handleConn(c io.ReadWriteCloser, sp *spool) {
	defer c.Close()

	// Read chunks and append to buffer.
	// Your C++ writes UTF-16LE bytes. We’ll store raw bytes and dump raw bytes.
	// If you want to convert to UTF-8 lines, see the note below.
	tmp := make([]byte, 32*1024)

	for {
		n, err := c.Read(tmp)
		if n > 0 {
			sp.append(tmp[:n])
		}
		if err != nil {
			// client disconnected or pipe broken
			if err != io.EOF {
				// Windows pipes often return ERROR_BROKEN_PIPE wrapped; EOF is fine too
				// Keep this quiet unless you want noisy logs.
			}
			return
		}
	}
}
