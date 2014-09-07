package ygor

// #cgo pkg-config: ygor
// #include <errno.h>
// #include <ygor.h>
import "C"
import "time"

type DataLogger struct {
	dl *C.struct_ygor_data_logger
}

func NewDataLogger(output string, scale_when uint64, scale_data uint64) (*DataLogger, error) {
	o := C.CString(output)
	dl, err := C.ygor_data_logger_create(o, C.uint64_t(scale_when), C.uint64_t(scale_data))
	if dl == nil {
		return nil, err
	}
	return &DataLogger{dl}, nil
}

func (d *DataLogger) FlushAndDestroy() error {
	x, err := C.ygor_data_logger_flush_and_destroy(d.dl)
	if x < 0 {
		return err
	}
	return nil
}

func (d *DataLogger) Record(series uint32, when uint64, data uint64) error {
	var dr C.struct_ygor_data_record
	dr.series = C.uint32_t(series)
	dr.when = C.uint64_t(when)
	dr.data = C.uint64_t(data)
	x, err := C.ygor_data_logger_record(d.dl, &dr)
	if x < 0 {
		return err
	}
	return nil
}

func (d *DataLogger) RecordNow(series uint32, data uint64) error {
	return d.Record(series, uint64(time.Now().UnixNano()), data)
}
