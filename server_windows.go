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
	"golang.org/x/sys/windows/svc/eventlog"
)

const (
	pipeName      = `\\.\pipe\lsass_log`
	dumpEvery     = 5 * time.Second
	outPath       = `C:\ProgramData\bang\pipe_dump.log`
	maxBufferSize = 10 * 1024 * 1024
)

type spool struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (s *spool) append(p []byte) {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.buf.Len()+len(p) > maxBufferSize {
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
	if err := os.MkdirAll(`C:\ProgramData\bang`, 0755); err != nil {
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
	if _, err := f.WriteString("\r\n"); err != nil {
		return err
	}
	s.buf.Reset()
	return nil
}

func runPipeBuf(stopCh <-chan struct{}, elog *eventlog.Log) error {
	logf := func(format string, a ...any) {
		msg := fmt.Sprintf(format, a...)
		if elog != nil {
			_ = elog.Info(1, msg)
		}
	}

	var sp spool

	// Periodic dumper
	go func() {
		t := time.NewTicker(dumpEvery)
		defer t.Stop()
		for {
			select {
			case <-t.C:
				if err := sp.dumpToFile(outPath); err != nil {
					if elog != nil {
						_ = elog.Error(1, "dump error: "+err.Error())
					}
				}
			case <-stopCh:
				_ = sp.dumpToFile(outPath) // final flush
				return
			}
		}
	}()

	ln, err := winio.ListenPipe(pipeName, &winio.PipeConfig{
		// Demo SDDL: Everyone full access. Tighten this for real use.
		SecurityDescriptor: "D:P(A;;GA;;;SY)(A;;GA;;;BA)",
		MessageMode:        false,
		InputBufferSize:    64 * 1024,
		OutputBufferSize:   64 * 1024,
	})
	if err != nil {
		return err
	}
	defer ln.Close()

	logf("listening on %s", pipeName)

	// Accept loop; stop by closing the listener when stopCh closes.
	go func() {
		<-stopCh
		_ = ln.Close()
	}()

	for {
		conn, err := ln.Accept()
		if err != nil {
			select {
			case <-stopCh:
				return nil
			default:
				time.Sleep(250 * time.Millisecond)
				continue
			}
		}
		go handleConn(conn, &sp)
	}
}

func handleConn(c io.ReadWriteCloser, sp *spool) {
	defer c.Close()
	tmp := make([]byte, 32*1024)
	for {
		n, err := c.Read(tmp)
		if n > 0 {
			sp.append(tmp[:n])
		}
		if err != nil {
			return
		}
	}
}
