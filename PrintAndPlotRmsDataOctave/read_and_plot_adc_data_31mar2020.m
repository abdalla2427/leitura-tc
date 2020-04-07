clear;
%GET text data from ESP32 using HTTP
URLstring = "192.168.11.7/adc_data";
[S,SUCCESS] = urlread(URLstring);
if (SUCCESS)
  %process adc_data vector, sampletime vector and i_rms from text
  data = strsplit(S,"!!");
  aux = strsplit (data{1,2}," ");
  adc_data = cellfun(@str2num,strsplit(data{1,1},","));
  sampletime = cellfun(@str2num,strsplit(aux{1,1},","));
  i_rms = str2num(aux{1,2});
  printf("Read %d ADC samples from URL %s, with i rms = %f.\n",size(adc_data,2),URLstring,i_rms);
  %plot i_rms_data with labels
  plot(sampletime,adc_data);
  ylabel("ADC data (counts)");
  xlabel("t (us)");
  grid on;
  title("ESP32 ADC samples");
  ylim([0 4096]);
  xlim([sampletime(1) sampletime(size(sampletime,2))]);
  text(sampletime(size(sampletime,2))/2,max(adc_data)+60,sprintf("i rms = %6.3f",i_rms));
  %evaluate time between samples
  aux2 = sampletime(2:size(sampletime,2));
  aux2 = aux2 .- sampletime(1:size(sampletime,2)-1);
  printf("Time between samples (us) - mean = %6.3f std = %6.3f min = %6.3f max = %6.3f\n", mean(aux2), std(aux2), min(aux2), max(aux2));
else
  printf("URL %s was not read",URLstring)
endif