clear;
%GET text data from ESP32 using HTTP
URLstring = "192.168.11.11/";
[S,SUCCESS] = urlread(URLstring);
if (SUCCESS)
  %process adc_data vector, sampletime vector and i_rms from text
  data = strsplit(S,"MUDOU");
  aux = strsplit (data{1,2}," ");
  adc_data = cellfun(@str2num,strsplit(data{1,1},","));
  sampletime = cellfun(@str2num,strsplit(aux{1,1},","));
  i_rms = str2num(aux{1,2});
  printf("Read %d ADC samples from URL %s, with i rms = %f.\n",size(adc_data,2),URLstring,i_rms);
  %adjust sampletime vector, subtracting the first value of each element
  %TODO: if roll over, consider element value before subtracting
  timeaxis = sampletime - sampletime(1); 
  %plot i_rms_data with labels
  plot(timeaxis,adc_data);
  ylabel("ADC data (counts)");
  xlabel("t (us)");
  grid on;
  title("ESP32 ADC samples");
  ylim([0 4096]);
  xlim([timeaxis(1) timeaxis(size(timeaxis,2))]);
  text(15000,max(adc_data)+60,sprintf("i rms = %f",i_rms));
else
  printf("URL %s was not read",URLstring)
endif