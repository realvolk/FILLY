package filly

import (
	"encoding/json"
	"fmt"
	"net"
	"os"
	"os/exec"
	"strings"
	"time"
)

type Session struct {
	socketPath string
	conn       net.Conn
	daemon     *exec.Cmd
}

type Response struct {
	Result    interface{} `json:"result"`
	Cancelled bool        `json:"cancelled"`
	Error     string      `json:"error"`
}

func NewSession(socketPath string) (*Session, error) {
	if socketPath == "" {
		socketPath = "/tmp/filly.sock"
	}
	// Auto-start daemon if socket doesn't exist
	if _, err := os.Stat(socketPath); os.IsNotExist(err) {
		cmd := exec.Command("filly", "daemon", "--socket", socketPath)
		cmd.Start()
		// Wait for socket to appear
		for i := 0; i < 50; i++ {
			if _, err := os.Stat(socketPath); err == nil {
				break
			}
			time.Sleep(50 * time.Millisecond)
		}
	}
	conn, err := net.Dial("unix", socketPath)
	if err != nil {
		return nil, err
	}
	return &Session{socketPath: socketPath, conn: conn}, nil
}

func (s *Session) Close() {
	if s.conn != nil {
		s.conn.Close()
	}
	if s.daemon != nil {
		s.daemon.Process.Kill()
	}
}

func (s *Session) send(widget string, params map[string]interface{}) (*Response, error) {
	req := map[string]interface{}{
		"widget": widget,
		"params": params,
	}
	data, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}
	data = append(data, '\n')
	if _, err := s.conn.Write(data); err != nil {
		return nil, err
	}
	buf := make([]byte, 65536)
	n, err := s.conn.Read(buf)
	if err != nil {
		return nil, err
	}
	// Handle streaming yields – skip lines starting with "yield:"
	response := strings.TrimSpace(string(buf[:n]))
	for strings.HasPrefix(response, "yield:") {
		idx := strings.IndexByte(response, '\n')
		if idx < 0 {
			break
		}
		response = response[idx+1:]
	}
	var resp Response
	if err := json.Unmarshal([]byte(response), &resp); err != nil {
		return nil, err
	}
	return &resp, nil
}

func (s *Session) Menu(title, message string, choices []string) (string, error) {
	resp, err := s.send("menu", map[string]interface{}{
		"title":   title,
		"message": message,
		"choices": choices,
	})
	if err != nil {
		return "", err
	}
	if resp.Cancelled {
		return "", fmt.Errorf("cancelled")
	}
	if s, ok := resp.Result.(string); ok {
		return s, nil
	}
	return "", fmt.Errorf("unexpected result type")
}

func (s *Session) YesNo(title, message string, def bool) (bool, error) {
	resp, err := s.send("yesno", map[string]interface{}{
		"title":   title,
		"message": message,
		"default": def,
	})
	if err != nil {
		return false, err
	}
	if resp.Cancelled {
		return false, fmt.Errorf("cancelled")
	}
	if b, ok := resp.Result.(bool); ok {
		return b, nil
	}
	return false, fmt.Errorf("unexpected result type")
}

func (s *Session) Input(title, message, def, placeholder string) (string, error) {
	params := map[string]interface{}{
		"title":   title,
		"message": message,
	}
	if def != "" {
		params["default"] = def
	}
	if placeholder != "" {
		params["placeholder"] = placeholder
	}
	resp, err := s.send("input", params)
	if err != nil {
		return "", err
	}
	if resp.Cancelled {
		return "", fmt.Errorf("cancelled")
	}
	if s, ok := resp.Result.(string); ok {
		return s, nil
	}
	return "", fmt.Errorf("unexpected result type")
}

func (s *Session) Password(title, message, placeholder string) (string, error) {
	params := map[string]interface{}{
		"title":   title,
		"message": message,
	}
	if placeholder != "" {
		params["placeholder"] = placeholder
	}
	resp, err := s.send("password", params)
	if err != nil {
		return "", err
	}
	if resp.Cancelled {
		return "", fmt.Errorf("cancelled")
	}
	if s, ok := resp.Result.(string); ok {
		return s, nil
	}
	return "", fmt.Errorf("unexpected result type")
}

func (s *Session) Msg(title, message string) error {
	_, err := s.send("msg", map[string]interface{}{
		"title":   title,
		"message": message,
	})
	return err
}