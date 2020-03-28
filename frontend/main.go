package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"github.com/0xAX/notificator"
	"github.com/denisbrodbeck/machineid"
	"github.com/getlantern/systray"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
)

type VersionResponse struct {
	Version_macos_catalina string
}

const Version = "0.1.0"

var backendMux sync.Mutex
var backendCmd *exec.Cmd
var backendOnIcon *systray.MenuItem
var backendOffIcon *systray.MenuItem
var backendRetry int
var backendOnline bool

func checkUpdate() (bool, error) {
	c := &http.Client{}
	req, err := http.NewRequest("GET", "https://magicmask.stanford.edu/latest-version.json", nil)
	if err != nil {
		return false, err
	}
	uuid, err := machineid.ProtectedID("magicMask")
	if err != nil {
		uuid = "NA"
	}
	req.Header.Add("MACHINE_ID", uuid)
	req.Header.Add("MASK_VERSION", Version)
	req.Header.Add("MACHINE_OS", runtime.GOOS)
	resp, err := c.Do(req)
	if err != nil {
		return false, err
	}
	var v VersionResponse
	d := json.NewDecoder(resp.Body)
	err = d.Decode(&v)
	resp.Body.Close()
	if err != nil {
		return false, err
	}
	return compareVersion(Version, v.Version_macos_catalina), nil
}

func compareVersion(us string, latest string) bool {
	usSplited := strings.Split(us, ".")
	latestSplited := strings.Split(latest, ".")
	var u [3]int
	var l [3]int
	for i := 0; i < 3; i++ {
		if i >= len(usSplited) {
			break
		}
		u[i], _ = strconv.Atoi(usSplited[i])
	}
	for i := 0; i < 3; i++ {
		if i >= len(latestSplited) {
			break
		}
		l[i], _ = strconv.Atoi(latestSplited[i])
	}
	// start compare
	for i := 0; i < 3; i++ {
		if l[i] > u[i] {
			return true
		}
		if u[i] > l[i] {
			return false
		}
	}
	return false
}

func handleConn(conn net.Conn) {
	notify := notificator.New(notificator.Options{
		AppName:     "Magic Mask",
		DefaultIcon: "icon_512x512.png",
	})

	r := bufio.NewScanner(conn)
	l := time.Unix(0, 0)
	for r.Scan() {
		line := r.Text()
		tokens := strings.Split(line, ",")
		if len(tokens) != 2 {
			continue
		}
		t := time.Now()
		d := t.Sub(l)
		l = t
		log.Printf("touched %v with %v", tokens[1], tokens[0])
		if d.Seconds() > 1.0 {
			notify.Push(fmt.Sprintf("You touched your %v!", tokens[1]), "Do not touch your face to avoid contracting a virus!", "icon_512x512.png", notificator.UR_NORMAL)
		}
		backendMux.Lock()
		backendRetry = 0
		backendMux.Unlock()
	}
}

func listen(port chan int) error {
	listener, err := net.Listen("tcp", "localhost:0")
	if err != nil {
		return err
	}
	p := listener.Addr().(*net.TCPAddr).Port
	port <- p

	for {
		conn, _ := listener.Accept()
		go handleConn(conn)
	}

	return nil
}

func turnOffBackend() {
	notify := notificator.New(notificator.Options{
		AppName:     "Magic Mask",
		DefaultIcon: "icon_512x512.png",
	})
	backendMux.Lock()
	if backendOnline {
		err := backendCmd.Process.Kill()
		if err != nil {
			notify.Push("Failed to quit camera monitoring", "Please restart Magic Mask!", "", notificator.UR_NORMAL)
		} else {
			backendOnline = false
			backendOnIcon.Uncheck()
			backendOffIcon.Check()
		}
	}
	backendMux.Unlock()
}

func turnOnBackend(port int) {
	notify := notificator.New(notificator.Options{
		AppName:     "Magic Mask",
		DefaultIcon: "icon_512x512.png",
	})
	backendMux.Lock()
	if !backendOnline {
		backendCmdTmo, err := startBackend(port)
		backendCmd = backendCmdTmo
		if err != nil {
			notify.Push("Failed to start camera monitoring", "Please restart Magic Mask!", "", notificator.UR_NORMAL)
		} else {
			backendOnline = true
			backendOnIcon.Check()
			backendOffIcon.Uncheck()
		}
	}
	backendMux.Unlock()
}

func startBackend(port int) (*exec.Cmd, error) {
	dir, err := filepath.Abs(filepath.Dir(os.Args[0]))
	if err != nil {
		return nil, err
	}
	backendPath := dir + "/MagicMask"
	pbtxtPath := dir + "/OWN_facehand_tracking_desktop_live.pbtxt"

	logPath := "/tmp/mimosa.out"
	logFile, _ := os.OpenFile(logPath, os.O_RDWR|os.O_CREATE, 0755)
	backend := exec.Command(backendPath, "--calculator_graph_config_file="+pbtxtPath, fmt.Sprintf("--notifications_port_str=%v", port))
	backend.Dir = dir
	backend.Stdout = logFile
	backend.Stderr = logFile
	err = backend.Start()
	if err != nil {
		return nil, err
	}
	notify := notificator.New(notificator.Options{
		AppName:     "Magic Mask",
		DefaultIcon: "icon_512x512.png",
	})
	go func() {
            time.Sleep(1 * time.Second)
		backend.Process.Wait()
		backendMux.Lock()
		if backendOnline != false {
			if backendRetry > 0 {
				backendRetry -= 1
				time.Sleep(1 * time.Second)
				backendCmd, _ = startBackend(port)
			} else {
				notify.Push("Camera monitoring exited", "Please start the camera monitoring from the menu!", "", notificator.UR_NORMAL)
				backendOnline = false
				backendOnIcon.Uncheck()
				backendOffIcon.Check()
			}
		}
		backendMux.Unlock()
	}()

	return backend, nil
}

func onReady() {
	systray.SetTitle("😷")
	pchan := make(chan int)
	go func() {
		err := listen(pchan)
		if err != nil {
			log.Printf("Frontend server crashed: %v\n", err)
		}
	}()
	port := <-pchan
	log.Printf("Frontend server started at %v\n", port)

	// portItem := systray.AddMenuItem(fmt.Sprintf("Port: %v", port), "")
	// portItem.Disable()
	// systray.AddSeparator()

	backendMux.Lock()
	backendOnIcon = systray.AddMenuItem("On", "")
	backendOffIcon = systray.AddMenuItem("Off", "")
	backendOffIcon.Check()
	backendCmdTmp, err := startBackend(port)
	backendCmd = backendCmdTmp
	if err != nil {
		log.Printf("Failed to start backend service: %v\n", err)
		systray.Quit()
	} else {
		log.Printf("Backend service started\n")
		backendOnline = true
		backendOnIcon.Check()
		backendOffIcon.Uncheck()
	}
	backendRetry = 60
	backendMux.Unlock()

	systray.AddSeparator()

	helpIcon := systray.AddMenuItem("Help", "")
	updateIcon := systray.AddMenuItem("Check for updates", "")
	quitIcon := systray.AddMenuItem("Quit", "")

	// start auto update
	t := time.Tick(time.Duration(6 * time.Hour))
	go func() {
		check := func() {
			u, err := checkUpdate()
			if err == nil && u == true {
				notify := notificator.New(notificator.Options{
					AppName:     "Magic Mask",
					DefaultIcon: "icon_512x512.png",
				})
				notify.Push("New version of Magic Mask available", "Please download the latest version of Magic Mask on: magicmask.stanford.edu", "", notificator.UR_NORMAL)
			}
		}
		check()
		for {
			select {
			case <-t:
				check()
			}
		}
	}()

	go func() {
		for {
			select {
			case <-quitIcon.ClickedCh:
				log.Println("Exiting")
				systray.Quit()
				return
			case <-backendOnIcon.ClickedCh:
				turnOnBackend(port)
			case <-backendOffIcon.ClickedCh:
				turnOffBackend()
			case <-helpIcon.ClickedCh:
				err := exec.Command("open", "https://magicmask.stanford.edu").Start()
				if err != nil {
					notify := notificator.New(notificator.Options{
						AppName:     "Magic Mask",
						DefaultIcon: "icon_512x512.png",
					})
					notify.Push("Failed to open browser", "For help, please visit: magicmask.stanford.edu", "", notificator.UR_NORMAL)
				}
			case <-updateIcon.ClickedCh:
				notify := notificator.New(notificator.Options{
					AppName:     "Magic Mask",
					DefaultIcon: "icon_512x512.png",
				})
				u, err := checkUpdate()
				if err != nil {
					notify.Push("Failed to check updates", "Please check for the latest version of Magic Mask on: magicmask.stanford.edu", "", notificator.UR_NORMAL)
					exec.Command("open", "https://magicmask.stanford.edu").Start()
				} else {
					if u {
						notify.Push("New version of Magic Mask available", "Please download the latest version of Magic Mask on: magicmask.stanford.edu", "", notificator.UR_NORMAL)
						exec.Command("open", "https://magicmask.stanford.edu").Start()
					} else {
						notify.Push("No updates available", "You are using the latest version of Magic Mask.", "", notificator.UR_NORMAL)
					}
				}
			}

		}
	}()
}

func main() {
	onExit := func() {
		backendMux.Lock()
		if backendOnline {
			err := backendCmd.Process.Kill()
			if err != nil {
				notify := notificator.New(notificator.Options{
					AppName:     "Magic Mask",
					DefaultIcon: "icon_512x512.png",
				})
				notify.Push("Failed to quit camera monitoring", "Please manually kill the process 'backend' in Activity Monitor.", "", notificator.UR_NORMAL)

			}
		}
		backendMux.Unlock()
		return
	}
	systray.Run(onReady, onExit)
}
