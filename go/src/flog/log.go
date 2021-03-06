package flog

type Log struct {
	loglevel LOGLEVEL
	logger *FileLog
}

func NewLogger(logpath string, logPrefix string, logname string, logLevel LOGLEVEL, logNum int, logSize int, shifttype SHIFT_TYPE, options ...interface{}) *Log {
	l := NewFileLog(logpath, logPrefix, logname, logLevel, logNum, logSize, shifttype)

	log := &Log {
		loglevel:logLevel,
		logger: l,
	}

	return log
}

func (this *Log) SetLogLevel(level LOGLEVEL) {
	this.loglevel = level
}

func (this *Log) Debug(format string, v ...interface{}) {
	if this.loglevel <= LOGLEVEL_DEBUG {
		this.logprint(LOGLEVEL_DEBUG, format, v...)
	}
}

func (this *Log) Info(format string, v ...interface{}) {
	if this.loglevel <= LOGLEVEL_INFO {
		this.logprint(LOGLEVEL_INFO, format, v...)
	}
}

func (this *Log) Warn(format string, v ...interface{}) {
	if this.loglevel <= LOGLEVEL_WARN {
		this.logprint(LOGLEVEL_WARN, format, v...)
	}
}

func (this *Log) Error(format string, v ...interface{}) {
	if this.loglevel <= LOGLEVEL_ERROR {
		this.logprint(LOGLEVEL_ERROR, format, v...)
	}
}

func (this *Log) Fatal(format string, v ...interface{}) {
	if this.loglevel <= LOGLEVEL_FATAL {
		this.logprint(LOGLEVEL_FATAL, format, v...)
	}
}

func (this *Log) Close() {
	this.logger.Close()
	//TODO
}

func (this *Log) logprint(level LOGLEVEL, format string, v ...interface{}) {
	this.logger.Logprint(level, format, v...)
}
