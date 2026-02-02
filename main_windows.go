//go:build windows

package main

import (
	"log"
	"os"

	"golang.org/x/sys/windows/svc"
	"golang.org/x/sys/windows/svc/eventlog"
)

func main() {
	isService, err := svc.IsWindowsService()
	if err != nil {
		log.Fatalf("IsWindowsService: %v", err)
	}

	if !isService {
		// Console mode (debug)
		log.Println("Running in console mode")
		if err := runPipeBufConsole(); err != nil {
			log.Fatalf("run: %v", err)
		}
		return
	}

	// Service mode: log to Windows Event Log
	elog, err := eventlog.Open("pipebuf")
	if err != nil {
		// fallback: nowhere nice to log, just exit
		return
	}
	defer elog.Close()

	elog.Info(1, "pipebuf service starting")
	if err := svc.Run("pipebuf", &pipebufService{elog: elog}); err != nil {
		elog.Error(1, "svc.Run failed: "+err.Error())
	}
	elog.Info(1, "pipebuf service stopped")
}

type pipebufService struct {
	elog *eventlog.Log
}

func (m *pipebufService) Execute(args []string, r <-chan svc.ChangeRequest, s chan<- svc.Status) (bool, uint32) {
	const accepts = svc.AcceptStop | svc.AcceptShutdown
	s <- svc.Status{State: svc.StartPending}
	stopCh := make(chan struct{})

	// Start your server
	go func() {
		if err := runPipeBuf(stopCh, m.elog); err != nil {
			m.elog.Error(1, "runPipeBuf: "+err.Error())
			// If you want, you can force service stop here by closing stopCh
			// close(stopCh)
		}
	}()

	s <- svc.Status{State: svc.Running, Accepts: accepts}

	for {
		select {
		case c := <-r:
			switch c.Cmd {
			case svc.Interrogate:
				s <- c.CurrentStatus
			case svc.Stop, svc.Shutdown:
				s <- svc.Status{State: svc.StopPending}
				close(stopCh)
				return false, 0
			default:
				// ignore
			}
		}
	}
}

// Console helper: stop on CTRL+C
func runPipeBufConsole() error {
	stopCh := make(chan struct{})
	go func() {
		// crude CTRL+C handler for console mode
		<-make(chan os.Signal, 1)
		close(stopCh)
	}()
	return runPipeBuf(stopCh, nil)
}
