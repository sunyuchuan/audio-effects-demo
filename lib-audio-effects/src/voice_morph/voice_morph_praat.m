function voice_morph_praat(infile, Frequency, Fmin, Fmax, FormantRatio, PitchRatio, PitchMedian, PitchRangeFactor, OutFileName)

[x,Fs] = wavread(infile);
if(Fs~=Frequency)
    return;
end

fid1 = fopen('pitch_track','wb');
fid2 = fopen('pitch_cand','wb');
fid3 = fopen('pitch_peak_track','wb');
fid4 = fopen('source_start_end_track','wb');

Decimation = 1;


FileLen = length(x);
PeriodsPerWin = 2;
FrameShiftFactor = 3;

FrameLen = PeriodsPerWin*Fs/Fmin;
FrameShift = FrameLen/FrameShiftFactor;
InterpolationDepth = 0.5;
MinLag = floor(Fs/Fmax/Decimation);
MaxLag = floor(Fs/Fmin/Decimation);
Window = hanning(FrameLen);
% a = 0.5:0.5/587:1;
% b = 1:-0.5/587:0.5;
% Window = [a,b];
Window = Window(:);
% Window(1) = 0;
% Window(end) = 0;
% Window = sqrt(Window);

VoiceThrd = 0.5;
IntensityThrd = 0.1;
FreqUpperThrd = 300;
PeakThrd = 0.01;
CorrDensityThrd = 15;
BrentPos = FrameLen*InterpolationDepth;

FrameBuf = zeros(FrameLen,1);
RawBuf = zeros(2*FrameShift,1);
MaxCorrCandNum = 10;
FittingDepth = 5;

DecFrameBuf = zeros(FrameLen/Decimation,1);
DecFrameShift = FrameShift/Decimation;
DecFrameLen = FrameLen/Decimation;

IntensCandSq = zeros(MaxCorrCandNum,1);
FreqCandSq = zeros(MaxCorrCandNum,1);
usedLen = FrameShift;
DecUsedLen = DecFrameShift;
Out = zeros(floor(2.5*length(x)),1);
outLen = 0;
CorrHan = xcorr(Window,'coeff');
InvCorrHan = floor(1./CorrHan);

BestCandFreq = 0;
LastPitch = 0;
PrevFreq = 0.0;
Confidence = 0;%succesive value confidence count
last_one = 0;%last value of a sequence

MorphBuf = zeros(9*FrameShift,1);% 4 core segments and 5 affiliate segments

LastPeakPos = 4*FrameShift;

FreqRef = 12.0*log(PitchMedian/100.0)/log(2);
seg_new_pitch = zeros(7,1);
seg_primary_pitch = zeros(7,1);

y = filter(0.04589*[1,-1.9884228,1],[1,-1.975961,0.9779],x(1:Decimation:end));
z = filter(0.0338585*[1,1],[1,-0.974798],y);
FrameCount = 0;

TargtAccLen = 0;
SourceAccLen = 0;
max_period_len = floor(Fs/Fmin*2);
last_joint_buf = zeros(max_period_len*0.5,1);
last_fall_buf = zeros(max_period_len,1);
last_joint_buf_len = max_period_len*0.5;
last_fall_len = max_period_len;
SourceAccPos = 4*FrameShift;

reference_pitch_len = 200;
first_pitch_record = zeros(reference_pitch_len,1);
first_pitch_ready = 0;
first_pitch_count = 0;
long_term_pitch = 0;
long_term_pitch_updated = 0;
pitch_vibrate_fraction = 0.4;

successive_pitch_seg_count = 0;
pitch_counting_flag = 0;
pitch_seg_thrd = 8;
pitch_inbuf_count = 0;
pitch_average_update_thrd = 50;
pitch_average_length = 0;

fid1_print_count = 0;

win1 = triang(1176/4);
CorrWin = xcorr(win1,'coeff');


while usedLen<=FileLen
    
    if usedLen==124*392
        usedLen = 124*392;
    end
    

    
    if Decimation~=1
        DecFrameBuf(1:DecFrameLen-DecFrameShift) = DecFrameBuf(DecFrameShift+1:end);
        DecFrameBuf(DecFrameLen-DecFrameShift+1:end) = z(DecUsedLen-DecFrameShift+1:DecUsedLen);
        LocalPeak = max(abs(DecFrameBuf));
        FilteredVec = (DecFrameBuf-sum(DecFrameBuf)/DecFrameLen);
        CorrVec = xcorr(FilteredVec,'coeff');
    %     CorrVec = CorrVec./CorrHan;
        CorrVec = CorrVec(DecFrameLen:end);
    else
        FrameBuf(1:FrameLen-FrameShift) = FrameBuf(FrameShift+1:end);
        FrameBuf(FrameLen-FrameShift+1:end) = z(usedLen-FrameShift+1:usedLen);
        LocalPeak = max(abs(FrameBuf));
        FilteredVec = (FrameBuf-sum(FrameBuf)/FrameLen);
        CorrVec = xcorr(FilteredVec(1:4:end),'coeff');
%         CorrVec = CorrVec./CorrWin;
        CorrVec = CorrVec(FrameLen/4:end);
    end
    RawBuf(1:FrameShift) = RawBuf(FrameShift+1:end);
    RawBuf(FrameShift+1:end) = x(usedLen-FrameShift+1:usedLen);
    
    FrameCount = FrameCount + 1;
    
    if FrameCount==5
        FrameCount = 5;
    end


%     FilteredVec = (FrameBuf-sum(FrameBuf)/FrameLen);
%     MainVec = FilteredVec(1:FrameLen/2);
%     MainVecNorm2 = MainVec'*MainVec;
%     CorrVec = zeros(FrameLen/2,1);
%     for i=1:FrameLen/2
%         CorrVec(i) = (MainVec'*FilteredVec(i:FrameLen/2+i-1))/MainVecNorm2;
%     end
    
%     CorrVec = xcorr((FrameBuf-sum(FrameBuf)/FrameLen),'coeff');

        

    CandNum = 0;
    Intensity = 0;
    count = 0;
    
    if LocalPeak>PeakThrd
        for i=2:MaxLag/4-2
            if CorrVec(i)>0.2*VoiceThrd && CorrVec(i)>CorrVec(i-1) && CorrVec(i)>CorrVec(i+1)
                dr = 0.5*(CorrVec(i+1) - CorrVec(i-1));
                d2r = 2*CorrVec(i) - CorrVec(i-1) - CorrVec(i+1);
                FreqEst = Fs/4*d2r/(i*d2r+dr);
                count = count + 1;
                if FreqEst<Fmax && FreqEst>Fmin
%                     Intensity = interpolate_sinc(CorrVec,FrameLen,Fs/FreqEst,FittingDepth);
                    Intensity = CorrVec(i);
                end

                if Intensity>=IntensityThrd && FreqEst<Fmax
                    CandNum = CandNum + 1;
                    FreqCandSq(CandNum) = FreqEst;
                    IntensCandSq(CandNum) = Intensity; 
                end  
                Intensity = 0;
            end        
        end
    end

    [BestCandFreq,LastPitch,Confidence,PrevFreq] = select_best_cand(IntensCandSq,FreqCandSq,CandNum,Fmin,PrevFreq,Confidence,LastPitch,first_pitch_ready,long_term_pitch,pitch_vibrate_fraction);

%     [BestCandFreq,LastPitch,Confidence,PrevFreq] = select_best_cand(IntensCandSq,FreqCandSq,CandNum,Fmin,PrevFreq,Confidence,LastPitch,first_pitch_ready,long_term_pitch,pitch_vibrate_fraction);
    if BestCandFreq~=0
        BestCandFreq = max(BestCandFreq,Fmin);
        if pitch_counting_flag==0          
            pitch_counting_flag = 1;
        end
        successive_pitch_seg_count = successive_pitch_seg_count + 1;
        pitch_inbuf_count = pitch_inbuf_count + 1;
        first_pitch_record(pitch_inbuf_count) = BestCandFreq;
    else
        if pitch_counting_flag==1 && successive_pitch_seg_count<=pitch_seg_thrd
            first_pitch_record(pitch_inbuf_count-successive_pitch_seg_count+1:pitch_inbuf_count) = zeros(successive_pitch_seg_count,1);
            pitch_inbuf_count = pitch_inbuf_count - successive_pitch_seg_count;                        
        end
        successive_pitch_seg_count = 0;
        pitch_counting_flag = 0;
    end
    
    if pitch_counting_flag==0 && pitch_inbuf_count>=pitch_average_update_thrd      
        [long_term_pitch,valid_num] = calculate_pitch_average(long_term_pitch,pitch_average_length,first_pitch_record,pitch_inbuf_count);
        pitch_average_length = pitch_average_length + valid_num;
        first_pitch_record = zeros(length(first_pitch_record),1);
        pitch_inbuf_count = 0;
        if first_pitch_ready==0
            first_pitch_ready = 1;
        end
    end

    PrimaryPitch = BestCandFreq;
%      PrimaryPitch = 300;

    if isnan(long_term_pitch)
        long_term_pitch = 0;
    end
    Pitch = PitchRatio*BestCandFreq;
    F = FreqRef + 12.0*(log(Pitch/PitchMedian)/log(2.0))*PitchRangeFactor;
    Pitch = 100.0*exp(F*(log(2.0)/12.0));
    if Pitch>0
        Pitch = max(Pitch,Fmin);
%         Pitch = 120.0;%robot
    end
    

%logarithm transform
%     a = 50/log(6);
%     b = 110 - a*log(75);
%     if BestCandFreq>0 && BestCandFreq<=110.0
%         Pitch = BestCandFreq;
%         
%     elseif BestCandFreq>110
%         Pitch = a*log(BestCandFreq) + b;
%     else 
%         Pitch = 0.0;
%     end    

%quadratic transform    
%     a = -175/(375*375);
%     b = -450;
%     c = 350;
%     if BestCandFreq>0
%         Pitch = a*(BestCandFreq+b)^2 + c;
%     else
%         Pitch = 0;
%     end
% linear transform1:小黄人
%     a = 300/375;
%     b = 200 - a*75;
%     if BestCandFreq>0
%         Pitch = a*BestCandFreq + b;
%     else
%         Pitch = 0;
%     end

%linear transform2
%     a = 50/450;
%     b = 100 - a*75;
%     if BestCandFreq>0 && BestCandFreq<=100.0
%         Pitch = BestCandFreq;
%         
%     elseif BestCandFreq>100
%         Pitch = a*BestCandFreq + b;
%     else 
%         Pitch = 0.0;
%     end
    
    %debug info: pitch
    if CandNum>0
        for j=1:CandNum
            fprintf(fid2,'(%f,%f) ',FreqCandSq(j),IntensCandSq(j));
        end
        fprintf(fid2,'\n');
    else
        fprintf(fid2,'(%f,%f)\n ',0.0,0.0);
    end
    fid1_print_count = fid1_print_count + 1;
    if fid1_print_count==141
        fid1_print_count = 141;
    end
    if count<CorrDensityThrd
        fprintf(fid1,'%f %d\n',BestCandFreq,count);
    else
        fprintf(fid1,'%f %d\n',0.0,count);
        fprintf(fid2,'%f\n',0.0);
    end
    %morph process
    MorphBuf(1:end-FrameShift) = MorphBuf(FrameShift+1:end);
    t = x(usedLen-FrameShift+1:usedLen);
    MorphBuf(end-FrameShift+1:end) = t;
    LastPeakPos = LastPeakPos - FrameShift;
    SourceAccPos = SourceAccPos - FrameShift;
    seg_primary_pitch(1) = seg_primary_pitch(2);
    seg_primary_pitch(2) = seg_primary_pitch(3);
    seg_primary_pitch(3) = seg_primary_pitch(4);
    seg_primary_pitch(4) = seg_primary_pitch(5);
    seg_primary_pitch(5) = seg_primary_pitch(6);
    seg_primary_pitch(6) = seg_primary_pitch(7);
    seg_primary_pitch(7) = PrimaryPitch;
    seg_new_pitch(1) = seg_new_pitch(2);
    seg_new_pitch(2) = seg_new_pitch(3);
    seg_new_pitch(3) = seg_new_pitch(4);
    seg_new_pitch(4) = seg_new_pitch(5);
    seg_new_pitch(5) = seg_new_pitch(6);
    seg_new_pitch(6) = seg_new_pitch(7);
    seg_new_pitch(7) = Pitch;
    
    fprintf(fid4,'Frame NO.:%d\n',FrameCount);
    [peak_seq,peak_count,pitch_flag,output,output_len,tgt_acc_len,src_acc_len,joint_buf,joint_buf_len,fall_buf,fall_buf_len,src_acc_pos] = pitch_shift_overlapadd(MorphBuf,seg_new_pitch,seg_primary_pitch,LastPeakPos,FormantRatio,FrameShift,...
                                                                                                                                                     last_joint_buf,last_joint_buf_len,last_fall_buf,...
                                                                                                                                                     last_fall_len,Fs,TargtAccLen,SourceAccLen,SourceAccPos,fid4);
    LastPeakPos = peak_seq(peak_count);
    Out(outLen+1:outLen + output_len) = output(1:output_len);
    outLen = outLen + output_len;
    TargtAccLen = tgt_acc_len;
    SourceAccLen = src_acc_len;
    last_joint_buf = joint_buf;
    last_joint_buf_len = joint_buf_len;
    last_fall_buf = fall_buf;
    last_fall_len = fall_buf_len;
    SourceAccPos = src_acc_pos;
    long_term_pitch_updated = 0;
    
    if outLen>8000
        outLen = outLen;
    end
    
    for r=1:peak_count
        fprintf(fid3,'%f    %d\n',usedLen-(7*FrameShift-peak_seq(r))-FrameShift,pitch_flag);
    end

    usedLen = usedLen + FrameShift;
    DecUsedLen = DecUsedLen + DecFrameShift;
    disp(FrameCount);
    disp(Pitch);
    
end
wavwrite(Out(1:outLen),Fs*FormantRatio,OutFileName);
fclose(fid1);
fclose(fid2);
fclose(fid3);
fclose(fid4);
disp(long_term_pitch);
end

function [val,last_indication,conf_val,prev_freq] = select_best_cand(IntensityVec, FreqCandVec, VecLen, Fmin, PrevFreq, Confidence, last_one, first_pitch_flag, pitch_mean, pitch_vibrate_fraction)
    if PrevFreq~=0.0
        UpperFreq = min(PrevFreq*1.2,450);
        LowerFreq = max(PrevFreq*0.8,Fmin);
    end
    
    f = 0.0;
    conf = 0;
    if VecLen==0 || VecLen>10%无候选
        if PrevFreq~=0.0 && last_one==0 && Confidence>4%延续一帧
            f = PrevFreq;
            conf = Confidence + 1;
            last_indication = 1;
        elseif PrevFreq~=0.0 && last_one==0 && Confidence<=4%置信度低于阈值，抛弃
            f = 0.0;
            last_indication = 1;
            conf = 0;
        elseif PrevFreq~=0.0 && last_one==1%last_one置位，复位
            f = 0.0;
            last_indication = 0;
            conf = 0;
        elseif PrevFreq==0.0%正常
            f = 0;
            last_indication = 0;
            conf = 0;
        end  
    elseif VecLen==1
%         if PrevFreq~=0.0
        if PrevFreq~=0.0 && Confidence>4%successive pitch cand
            if first_pitch_flag==0
                f = FreqCandVec(1);
            else
                if FreqCandVec(1)>=LowerFreq && FreqCandVec(1)<=UpperFreq
                    f = FreqCandVec(1);
                elseif FreqCandVec(1)>1.75*PrevFreq
                    mul = round(FreqCandVec(1)/PrevFreq);
                    f = FreqCandVec(1)/mul;
                elseif FreqCandVec(1)>UpperFreq && FreqCandVec(1)<1.75*PrevFreq
                    f = UpperFreq;
                elseif FreqCandVec(1)<LowerFreq && FreqCandVec(1)<0.7*PrevFreq
                    mul = round(PrevFreq/FreqCandVec(1));
                    f = FreqCandVec(1)*mul;
                elseif FreqCandVec(1)<LowerFreq && FreqCandVec(1)>=0.7*PrevFreq
                    f = LowerFreq;
                end
            end
            
            conf = Confidence + 1;
        elseif PrevFreq~=0.0 && Confidence<=4
            f = FreqCandVec(1);
%             if first_pitch_flag==0
%                 if FreqCandVec(1)>=LowerFreq && FreqCandVec(1)<=UpperFreq
%                     f = FreqCandVec(1);
%                 elseif FreqCandVec(1)>1.75*PrevFreq
%                     mul = round(FreqCandVec(1)/PrevFreq);
%                     f = FreqCandVec(1)/mul;
%                 elseif FreqCandVec(1)>UpperFreq && FreqCandVec(1)<1.75*PrevFreq
%                     f = UpperFreq;
%                 elseif FreqCandVec(1)<LowerFreq && FreqCandVec(1)<0.7*PrevFreq
%                     mul = round(PrevFreq/FreqCandVec(1));
%                     f = FreqCandVec(1)*mul;
%                 elseif FreqCandVec(1)<LowerFreq && FreqCandVec(1)>=0.7*PrevFreq
%                     f = LowerFreq;
%                 end
%             else
%                 if FreqCandVec(1)<=(1+pitch_vibrate_fraction)*pitch_mean && FreqCandVec(1)>=(1-pitch_vibrate_fraction)*pitch_mean
%                     f = FreqCandVec(1);
%                 elseif FreqCandVec(1)>(1+pitch_vibrate_fraction)*pitch_mean
%                     mul = round(FreqCandVec(1)/pitch_mean);
%                     f = FreqCandVec(1)/mul;
%                 elseif FreqCandVec(1)<(1-pitch_vibrate_fraction)*pitch_mean
%                     mul = round(pitch_mean/FreqCandVec(1));
%                     f = FreqCandVec(1)*mul;
%                 end 
%             end
                      
            conf = Confidence + 1;
        elseif PrevFreq==0.0%first pitch in a seg
            if first_pitch_flag==0
                f = FreqCandVec(1);
            else
                if FreqCandVec(1)<=(1+pitch_vibrate_fraction)*pitch_mean && FreqCandVec(1)>=(1-pitch_vibrate_fraction)*pitch_mean
                    f = FreqCandVec(1);
                elseif FreqCandVec(1)>(1+pitch_vibrate_fraction)*pitch_mean
                    mul = round(FreqCandVec(1)/pitch_mean);
                    f = FreqCandVec(1)/mul;
                elseif FreqCandVec(1)<(1-pitch_vibrate_fraction)*pitch_mean
                    mul = round(pitch_mean/FreqCandVec(1));
                    f = FreqCandVec(1)*mul;
                end 
            end
            conf = 1;
        end
        last_indication = 0;
    else
        max_val = IntensityVec(1);
        pos = 1;
        for i=2:VecLen
            if IntensityVec(i)>max_val
                max_val = IntensityVec(i);
                pos = i;
            end
        end  
%         if PrevFreq~=0.0
        if PrevFreq~=0.0 && Confidence>4
            if first_pitch_flag==0
                max_val = 0;
                possible_pos = -1;
                for i=1:VecLen
                    delta2 = abs(PrevFreq - FreqCandVec(i));
                    if delta2<PrevFreq
                        part2 = 1-abs(PrevFreq - FreqCandVec(i))/PrevFreq;
                    else
                        part2 = 0;
                    end
                    
                    score = part2*0.5 + IntensityVec(i)*0.5;
                    if score>max_val
                        max_val = score;
                        possible_pos = i;
                    end
                end
                f = FreqCandVec(possible_pos);
            else
                max_val = 0;
                possible_pos = -1;
                for i=1:VecLen
%                     tmp = (1-abs(pitch_mean - FreqCandVec(i))/pitch_mean)*0.35 + (1-abs(PrevFreq - FreqCandVec(i))/PrevFreq)*0.35 + IntensityVec(i)*0.3;
                    delta1 = abs(pitch_mean - FreqCandVec(i));
                    if delta1<pitch_mean
                        part1 = 1-abs(pitch_mean - FreqCandVec(i))/pitch_mean;
                    else
                        part1 = 0;
                    end
                    
                    delta2 = abs(PrevFreq - FreqCandVec(i));
                    if delta2<PrevFreq
                        part2 = 1-abs(PrevFreq - FreqCandVec(i))/PrevFreq;
                    else
                        part2 = 0;
                    end
                    
                    score = part1*0.1 + part2*0.1 + IntensityVec(i)*0.8;
                    if score>max_val
                        max_val = score;
                        possible_pos = i;
                    end
                end
                f = FreqCandVec(possible_pos);
            end
%             if FreqCandVec(pos)>=LowerFreq && FreqCandVec(pos)<=UpperFreq
%                 f = FreqCandVec(pos);
%             elseif FreqCandVec(pos)>UpperFreq || FreqCandVec(pos)<LowerFreq
%                 [cand,sec_pos] = select_second_best_cand(pos,FreqCandVec,VecLen,LowerFreq,UpperFreq);
%                 if sec_pos>0
%                     f = cand;
%                 else
%                     if first_pitch_flag==0 && FreqCandVec(pos)>UpperFreq
%                         f = UpperFreq;
%                     elseif first_pitch_flag==0 && FreqCandVec(pos)<LowerFreq
%                         f = LowerFreq;
%                     elseif first_pitch_flag==1
%                         min_val = 1000;
%                         possible_pos = -1;
%                         for i=1:VecLen
%                             tmp = abs(pitch_mean - FreqCandVec(i));
%                             if tmp<min_val
%                                 min_val = tmp;
%                                 possible_pos = i;
%                             end
%                         end
%                         f = FreqCandVec(possible_pos);                      
%                     end                    
%                 end
%             end
            conf = Confidence + 1;
        elseif PrevFreq~=0.0 && Confidence<=4
            if first_pitch_flag==0
                max_val = 0;
                possible_pos = -1;
                for i=1:VecLen
%                     tmp = (1-abs(pitch_mean - FreqCandVec(i))/pitch_mean)*0.35 + (1-abs(PrevFreq - FreqCandVec(i))/PrevFreq)*0.35 + IntensityVec(i)*0.3;
                    delta1 = abs(pitch_mean - FreqCandVec(i));
                    if delta1<pitch_mean
                        part1 = 1-abs(pitch_mean - FreqCandVec(i))/pitch_mean;
                    else
                        part1 = 0;
                    end
                    
%                     delta2 = abs(PrevFreq - FreqCandVec(i));
%                     if delta2<PrevFreq
%                         part2 = 1-abs(PrevFreq - FreqCandVec(i))/PrevFreq;
%                     else
%                         part2 = 0;
%                     end
                    
                    score = part1*0.4 + IntensityVec(i)*0.6;
                    if score>max_val
                        max_val = score;
                        possible_pos = i;
                    end
                end
                f = FreqCandVec(possible_pos);
            else
                max_val = 0;
                possible_pos = -1;
                for i=1:VecLen
%                     tmp = (1-abs(pitch_mean - FreqCandVec(i))/pitch_mean)*0.35 + (1-abs(PrevFreq - FreqCandVec(i))/PrevFreq)*0.35 + IntensityVec(i)*0.3;
                    delta1 = abs(pitch_mean - FreqCandVec(i));
                    if delta1<pitch_mean
                        part1 = 1-abs(pitch_mean - FreqCandVec(i))/pitch_mean;
                    else
                        part1 = 0;
                    end
                    
                    delta2 = abs(PrevFreq - FreqCandVec(i));
                    if delta2<PrevFreq
                        part2 = 1-abs(PrevFreq - FreqCandVec(i))/PrevFreq;
                    else
                        part2 = 0;
                    end
                    
                    score = part1*0.2 + part2*0.2 + IntensityVec(i)*0.6;
                    if score>max_val
                        max_val = score;
                        possible_pos = i;
                    end
                end
                f = FreqCandVec(possible_pos);
            end          
            conf = Confidence + 1;
        elseif PrevFreq==0.0           
            if first_pitch_flag==0
                f = FreqCandVec(pos);
            else
                max_val = 0;
                possible_pos = -1;
%                 ind = zeros(VecLen,1);
                for i=1:VecLen
                    part1 = (1-abs(pitch_mean - FreqCandVec(i))/pitch_mean);
                    if part1<0 %far away from pitch_mean,impossible
                        part1 = 0;
                    end
                    tmp = part1*0.3 + IntensityVec(i)*0.7;
                    if tmp>max_val
                        max_val = tmp;
                        possible_pos = i;
                    end
                end
%                 min_val = 10000;
%                 if possible_pos==-1
%                     for i=1:VecLen
%                         value = abs(FreqCandVec(i) - pitch_mean)/pitch_mean;
%                         if value<min_val
%                             min_val = value;
%                             possible_pos = i;
%                         end
%                     end
%                 end
                f = FreqCandVec(possible_pos);
            end

%             f = FreqCandVec(pos);

            conf = 1;
        end
        last_indication = 0;
    end 
    prev_freq = f;
    val = f;
    conf_val = conf;
    
end

function [sec_cand,pos] = select_second_best_cand(BestPos,DataVec,VecLen,Fmin,Fmax)
    pos = -1;
    sec_cand = 0.0;
%     if first_pitch_flag==0
%         for i=1:VecLen
%             if DataVec(i)>=Fmin && DataVec(i)<=Fmax && i~=BestPos
%                 pos = i;
%                 sec_cand = DataVec(i);
%             end
%         end 
%     else
%         for i=1:VecLen
%             if i~=BestPos && DataVec(i)<1.5*pitch_mean && DataVec(i)>0.5*pitch_mean
%                 pos = i;
%                 sec_cand = DataVec(i);
%             end
%         end 
%     end
    
     for i=1:VecLen
        if DataVec(i)>=Fmin && DataVec(i)<=Fmax && i~=BestPos
            pos = i;
            sec_cand = DataVec(i);
        end
    end
     
end

function [pitch_mean,num] = calculate_pitch_average(last_mean,last_len,pitch_array,array_len)
    invalid_pitch_sum = 0;
    invalid_pitch_count = 0;
    if last_mean==0
        mean_val = sum(pitch_array)/array_len;
        for i=1:array_len
            if pitch_array(i)>1.75*mean_val || pitch_array(i)<0.25*mean_val%magnitude should be in range [-50%,50%] of mean
                invalid_pitch_count = invalid_pitch_count + 1;
                invalid_pitch_sum = invalid_pitch_sum + pitch_array(i);
            end
        end
        num = array_len-invalid_pitch_count;
        pitch_mean = (sum(pitch_array)-invalid_pitch_sum)/num;
    else
        for i=1:array_len
            if pitch_array(i)>1.4*last_mean || pitch_array(i)<0.6*last_mean%magnitude should be in range [-50%,50%] of mean
                invalid_pitch_count = invalid_pitch_count + 1;
                invalid_pitch_sum = invalid_pitch_sum + pitch_array(i);
            end
        end
        num = array_len-invalid_pitch_count;
        pitch_mean = (sum(pitch_array)-invalid_pitch_sum + last_mean*last_len)/(num+last_len);
    end
    
end
%    affiliate               core segment                 affiliate
%/---------------\/------------------------------\/---------------------\
%|_______|_______|_______|_______|_______|_______|_______|_______|_______|
%                    |                                               |
%                    |                                               |
%                   seg1                                            seg7        

function [peak_seq,peak_count,pitch_flag,output,output_len,tgt_acc_len,src_acc_len,joint_buf,joint_buf_len,fall_buf,fall_buf_len,last_src_pos] = pitch_shift_overlapadd(PrcsBuf,SegPitch,SegPrimaryPitch,LastPeakPos,ratio,ShiftLen,...
                                                                                                                                                             last_joint_buf,last_joint_buf_len,last_fall_buf,...
                                                                                                                                                             last_fall_len,Fs,TargtAccLen,SourceAccLen,src_acc_pos,...
                                                                                                                                                             debug_file_id)
    peak_seq = zeros(ceil(ShiftLen*3/(Fs/450)),1);
    peak_count = 0;
    freq_seq = zeros(ceil(ShiftLen*3/(Fs/450)),1);
    pitch_flag = 0;
    noise_period = 0.004*Fs;
    output_len = 0;
    output = [];
    if SegPitch(2)==0.0        
        %LastPeakPos = length(PrcsBuf)-2*ShiftLen;
        if src_acc_pos<4*ShiftLen
            %not in target seg
            prcs_start = src_acc_pos+1;
            prcs_end = 4*ShiftLen;
%             [peak,output_unvoiced,output_unvoiced_len] = process_one_unvoiced_interval(PrcsBuf,LastPeakPos,prcs_start,prcs_end,ratio,ShiftLen);
            LastFreq = get_seg_freq(src_acc_pos,SegPitch,ShiftLen);
            [peak,output_unvoiced,output_unvoiced_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,src_pos] = process_one_unvoiced_interval(PrcsBuf,LastPeakPos,LastFreq,prcs_start,prcs_end,...
                                                                                                                    ratio,noise_period,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                    last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            peak_count = 1;
            peak_seq(1) = peak;
            pitch_flag = 0; 
             
            output(output_len+1:output_len + output_unvoiced_len - joint_buf_len - fall_buf_len) = output_unvoiced(1:output_unvoiced_len - joint_buf_len - fall_buf_len);  
            output_len = output_len + output_unvoiced_len - joint_buf_len - fall_buf_len;
            joint_buf = output_unvoiced(output_unvoiced_len-joint_buf_len-fall_buf_len+1:output_unvoiced_len-fall_buf_len);
            fall_buf = output_unvoiced(output_unvoiced_len-fall_buf_len+1:output_unvoiced_len);
            last_src_pos = src_pos;
        elseif src_acc_pos>=4*ShiftLen
            peak_count = 1;
            peak_seq(1) = src_acc_pos;
            pitch_flag = 0;   
            joint_buf = last_joint_buf;
            fall_buf = last_fall_buf;
            joint_buf_len = last_joint_buf_len;
            fall_buf_len = last_fall_len;
            tgt_acc_len = TargtAccLen;
            src_acc_len = SourceAccLen;
            last_src_pos = src_acc_pos;
        end       
%         tgt_acc_len = 0;
%         src_acc_len = 0;
    elseif SegPitch(1)~=0.0 && SegPitch(2)~=0.0
        %successive pitch interval
        Period = floor(Fs/SegPrimaryPitch(2));
        peak_pos = -1;
        peak_freq = -1;
        count = 0;
        region = -1;
        search_pos = LastPeakPos;
        if LastPeakPos<=4*ShiftLen
            if (0.8*Period+LastPeakPos)<=(4*ShiftLen)            
                while region<=(4*ShiftLen)
                    [test_seq,test_seq_num,peak_pos] = get_next_pitch_peak(PrcsBuf,search_pos,Period,0.8*Period,1.25*Period,ShiftLen);
                    count = count + 1;
                    peak_seq(count) = test_seq(peak_pos);
                    region = test_seq(peak_pos);
                    search_pos = test_seq(peak_pos);
                    freq_seq(count) = get_seg_freq(peak_seq(count),SegPitch,ShiftLen);
                end
            else 
                [test_seq,test_seq_num,peak_pos] = get_next_pitch_peak(PrcsBuf,LastPeakPos,Period,0.8*Period,1.25*Period,ShiftLen);
                count = 1;
                freq_seq(count) = get_seg_freq(test_seq(peak_pos),SegPitch,ShiftLen);
            end
        elseif LastPeakPos>4*ShiftLen && LastPeakPos<=5*ShiftLen
            count = 0;
            peak_pos = 1;
            test_seq(peak_pos) = LastPeakPos;
        else %(0.8*Period+LastPeakPos)>(length(PrcsBuf)-2*ShiftLen) && (1.25*Period+LastPeakPos)<=(length(PrcsBuf)-ShiftLen)
            disp('impossible branch!');
%             peak_pos = get_next_pitch_peak(PrcsBuf,LastPeakPos,Period,0.8*Period,1.25*Period,ShiftLen);
%             peak_freq = get_seg_freq(peak_pos,SegPitch);
%             count = 1;
        end
        if count==0 
%             process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_pos,peak_freq);
            peak_seq(1) = test_seq(peak_pos);
            peak_count = 1;
            output_len = 0;
            tgt_acc_len = TargtAccLen;
            src_acc_len = SourceAccLen;
            joint_buf = last_joint_buf;
            joint_buf_len = last_joint_buf_len;
            fall_buf = last_fall_buf;
            fall_buf_len = last_fall_len;
            last_src_pos = src_acc_pos;
        elseif count==1
            peak_seq(1) = test_seq(peak_pos);
            peak_count = 1;
            LastPeakFreq = get_seg_freq(LastPeakPos,SegPitch,ShiftLen);
            [peak,output_voiced,output_voiced_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,src_pos] = process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,peak_count,...
                                                                                                                                    ratio,noise_period,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                                    last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            output(output_len+1:output_len + output_voiced_len - joint_buf_len - fall_buf_len) = output_voiced(1:output_voiced_len - joint_buf_len - fall_buf_len);
            output_len = output_len + output_voiced_len - joint_buf_len - fall_buf_len;
            joint_buf = output_voiced(output_voiced_len-joint_buf_len-fall_buf_len+1:output_voiced_len-fall_buf_len);
            fall_buf = output_voiced(output_voiced_len-fall_buf_len+1:output_voiced_len);
            last_src_pos = src_pos;
        elseif count>1
            LastPeakFreq = get_seg_freq(LastPeakPos,SegPitch,ShiftLen);
            [peak,output_voiced,output_voiced_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,src_pos] = process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,count,...
                                                                                                                                    ratio,noise_period,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                                    last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            output(output_len+1:output_len + output_voiced_len - joint_buf_len - fall_buf_len) = output_voiced(1:output_voiced_len - joint_buf_len - fall_buf_len);
            output_len = output_len + output_voiced_len - joint_buf_len - fall_buf_len;
            joint_buf = output_voiced(output_voiced_len-joint_buf_len-fall_buf_len+1:output_voiced_len-fall_buf_len);
            fall_buf = output_voiced(output_voiced_len-fall_buf_len+1:output_voiced_len);
            peak_count = count;
            last_src_pos = src_pos;
        end
        pitch_flag = 1;
    elseif SegPitch(1)==0.0 && SegPitch(2)~=0.0
        %start of a voiced interval      
        peak_pos = -1;
        peak_freq = -1;
        region = -1;
        count = 0;
        out_ptr = 0;
        if LastPeakPos<=4*ShiftLen
            local_peak_pos = find_local_peak(PrcsBuf(3*ShiftLen+1:4*ShiftLen));    
            Period = floor(Fs/SegPrimaryPitch(2));
            search_pos = local_peak_pos+3*ShiftLen;
            if search_pos<=LastPeakPos
                LastPeakPos = 3*ShiftLen;
                src_acc_pos = 3*ShiftLen;
            end
%             [output_uv,output_uv_len] =
%             process_unvoiced_to_voiced_interval(PrcsBuf,LastPeakPos,search_pos,SegPitch(2),0.008*44100,ratio,Fs);
            [output_uv,output_uv_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,unvoiced_pos] = process_unvoiced_to_voiced_interval(PrcsBuf,LastPeakPos,search_pos,0,...
                                                                                                                            noise_period,ratio,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                            last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            LastPeakPos = search_pos;
            output = [output;output_uv(1:output_uv_len - joint_buf_len - fall_buf_len)];
            output_len = output_len + output_uv_len - joint_buf_len - fall_buf_len;
            joint_buf = output_uv(output_uv_len-joint_buf_len-fall_buf_len+1:output_uv_len-fall_buf_len);
            fall_buf = output_uv(output_uv_len-fall_buf_len+1:output_uv_len);
            src_acc_pos = unvoiced_pos;
            SourceAccLen = src_acc_len;
            if (0.8*Period+search_pos)<=(4*ShiftLen) 
                while region<=(4*ShiftLen)
                    [test_seq,test_seq_num,peak_pos] = get_next_pitch_peak(PrcsBuf,search_pos,Period,0.8*Period,1.25*Period,ShiftLen);
                    count = count + 1;
                    peak_seq(count) = test_seq(peak_pos);
                    region = test_seq(peak_pos);
                    search_pos = test_seq(peak_pos);
                    freq_seq(count) = get_seg_freq(peak_seq(count),SegPitch,ShiftLen);
                end
            else 
                [test_seq,test_seq_num,peak_pos] = get_next_pitch_peak(PrcsBuf,search_pos,Period,0.8*Period,1.25*Period,ShiftLen);
                freq_seq(1) = get_seg_freq(test_seq(peak_pos),SegPitch,ShiftLen);
                count = 1;
            end
        elseif LastPeakPos>4*ShiftLen && LastPeakPos<=5*ShiftLen
            count = 0;
            peak_pos = 1;
            test_seq(peak_pos) = LastPeakPos;
        else %(0.8*Period+LastPeakPos)>(length(PrcsBuf)-2*ShiftLen) && (1.25*Period+LastPeakPos)<=(length(PrcsBuf)-ShiftLen)
            disp('impossible last peak position!');
        end
        
        if count==0 
%             process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_pos,peak_freq);
            peak_seq(1) = test_seq(peak_pos);
            peak_count = 1;
            output_len = 0;
            tgt_acc_len = TargtAccLen;
            src_acc_len = SourceAccLen;
            joint_buf = last_joint_buf;
            joint_buf_len = last_joint_buf_len;
            fall_buf = last_fall_buf;
            fall_buf_len = last_fall_len;
            last_src_pos = src_acc_pos;
        elseif count==1
            peak_seq(1) = test_seq(peak_pos);
            peak_count = 1;
            LastPeakFreq = get_seg_freq(LastPeakPos,SegPitch,ShiftLen);
%             [peak,output_voiced,output_voiced_len] = process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,peak_count,ratio,44100*0.008,ShiftLen,Fs);  
            [peak,output_voiced,output_voiced_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,src_pos] = process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,peak_count,...
                                                                                                                    ratio,noise_period,ShiftLen,Fs,joint_buf,joint_buf_len,...
                                                                                                                    fall_buf,fall_buf_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            output(output_len+1:output_len + output_voiced_len - joint_buf_len - fall_buf_len) = output_voiced(1:output_voiced_len - joint_buf_len - fall_buf_len);
            output_len = output_len + output_voiced_len - joint_buf_len - fall_buf_len;
            joint_buf = output_voiced(output_voiced_len-joint_buf_len-fall_buf_len+1:output_voiced_len-fall_buf_len);
            fall_buf = output_voiced(output_voiced_len-fall_buf_len+1:output_voiced_len);
            last_src_pos = src_pos;
        elseif count>1
            LastPeakFreq = get_seg_freq(LastPeakPos,SegPitch,ShiftLen);
            [peak,output_voiced,output_voiced_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,src_pos] = process_one_voiced_interval(PrcsBuf,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,count,...
                                                                                                                    ratio,noise_period,ShiftLen,Fs,joint_buf,joint_buf_len,...
                                                                                                                    fall_buf,fall_buf_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id);
            output(output_len+1:output_len + output_voiced_len - joint_buf_len - fall_buf_len) = output_voiced(1:output_voiced_len - joint_buf_len - fall_buf_len);
            output_len = output_len + output_voiced_len - joint_buf_len - fall_buf_len;
            joint_buf = output_voiced(output_voiced_len-joint_buf_len-fall_buf_len+1:output_voiced_len-fall_buf_len);
            fall_buf = output_voiced(output_voiced_len-fall_buf_len+1:output_voiced_len);
            peak_count = count;
            last_src_pos = src_pos;
        end
        pitch_flag = 1;
    end
    
    if peak_count==0
        peak_count = 0;
    end
end

function p = find_local_peak(buf)
    len = length(buf);
    max_val = -1;
    min_val = 1;
    for i=1:len
        if buf(i)>max_val
            max_val = buf(i);
            pos1 = i;
        end
        
        if buf(i)<min_val
            min_val = buf(i);
            pos2 = i;
        end           
    end
    
    if abs(max_val)>=abs(min_val)
        p = pos1;
    else
        p = pos2;
    end
end

function [pitch_peak,pitch_peak_len,target_peak_pos] = get_next_pitch_peak(PrcsBuf,LastPeakPos,LastPeriod,LowPos,HighPos,ShiftLen)
%     max_peak_num = floor((length(PrcsBuf)-ShiftLen-LastPeakPos)/Period);
    start = floor(LastPeakPos+LowPos+1);
    stop = ceil(LastPeakPos+HighPos-1);
    stop = min(length(PrcsBuf),stop);

    pitch_peak = zeros(floor(HighPos-LowPos+1),1);
    pitch_peak_len = 0;
    
%     if LastPeakPos>=0
%         for i=(start+1):(stop-1)
%             if PrcsBuf(i)>=0 && PrcsBuf(i)>=PrcsBuf(i-1) && PrcsBuf(i)>=PrcsBuf(i+1)
%                 pitch_peak_len = pitch_peak_len + 1;
%                 pitch_peak(pitch_peak_len) = i;
%             end
%         end
%     else
%         for i=(start+1):(stop-1)
%             if PrcsBuf(i)<0 && PrcsBuf(i)<=PrcsBuf(i-1) && PrcsBuf(i)<=PrcsBuf(i+1)
%                 pitch_peak_len = pitch_peak_len + 1;
%                 pitch_peak(pitch_peak_len) = i;             
%             end
%         end
%     end
    
    if LastPeakPos>=0
        for i=(start+1):(stop-1)
            if PrcsBuf(i)>=0 && PrcsBuf(i)>=PrcsBuf(i-1) && PrcsBuf(i)>=PrcsBuf(i+1)
                pitch_peak_len = pitch_peak_len + 1;
                pitch_peak(pitch_peak_len) = i;    
            elseif PrcsBuf(i)<0 && PrcsBuf(i)<=PrcsBuf(i-1) && PrcsBuf(i)<=PrcsBuf(i+1)
                pitch_peak_len = pitch_peak_len + 1;
                pitch_peak(pitch_peak_len) = i;             
            end
        end
    end
    
    if pitch_peak_len==0
        if abs(PrcsBuf(start+1))>abs(PrcsBuf(stop-1))
            pitch_peak_len = 1; 
            pitch_peak(pitch_peak_len) = start+1;
        else
            pitch_peak_len = 1;
            pitch_peak(pitch_peak_len) = stop+1;
        end
    end
    
    half_peroid = floor(LastPeriod/2);
    max_val = -1;
    pos = -1;
    left_pos1 = max(LastPeakPos-half_peroid,0);
    right_pos1 = min(LastPeakPos+half_peroid,length(PrcsBuf));
    left_len1 = LastPeakPos - left_pos1;
    right_len1 = right_pos1 - LastPeakPos;
    for q=1:pitch_peak_len
        left_pos2 = max(pitch_peak(q)-half_peroid,0);
        right_pos2 = min(pitch_peak(q)+half_peroid,length(PrcsBuf));
        left_len2 = pitch_peak(q) - left_pos2;
        right_len2 = right_pos2 - pitch_peak(q);
        left_len = min(left_len1,left_len2);
        right_len = min(right_len1,right_len2);
        tmp = PrcsBuf(LastPeakPos-left_len:LastPeakPos+right_len);
        tmp1 = PrcsBuf(pitch_peak(q)-left_len:pitch_peak(q)+right_len);
        crs_term = tmp1'*tmp;
        norm1 = tmp'*tmp;
        norm2 = tmp1'*tmp1;
        crr_val = crs_term/sqrt(norm1*norm2+eps);
        if crr_val>max_val
            max_val = crr_val;
            pos = q;
        end
    end  
    target_peak_pos = pos;
end

function frequency = get_seg_freq(Position,Pitch,ShiftLen)
    pos = ceil(Position/ShiftLen-2);
    if pos==0
        pos = 1;
    end
    frequency = Pitch(pos);
        
end

function [peak,output,output_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,unvoiced_pos] = process_one_unvoiced_interval(data,LastPeakPos,LastPeakFreq,prcs_start,prcs_end,...
                                                                                                                    ratio,noise_period,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                    last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id)
    stretched_period = floor(noise_period*ratio);
    period = floor(noise_period);
    num = floor((prcs_end - src_acc_pos)/period);  
    output = zeros(3*ShiftLen*2+last_joint_buf_len,1);
    output_len = 0;
    output(output_len+1:output_len+last_joint_buf_len) = last_joint_buf;
    output_len = output_len + last_joint_buf_len;
    output(output_len+1:output_len+last_fall_len) = last_fall_buf;
    output_len = output_len + last_fall_len;
    
    if num==0
        peak = LastPeakPos;
        tgt_acc_len = TargtAccLen;
        src_acc_len = SourceAccLen;
        joint_buf_len = last_joint_buf_len;
        fall_buf_len = last_fall_len;
        unvoiced_pos = src_acc_pos;
        return;
    end
    tgt_pos = TargtAccLen;
    src_pos = SourceAccLen;
    pos = src_acc_pos;
  
    stretched_pos = 0;
    for i=1:num
        pos = pos + period;
        dphase = pi/stretched_period;
        fall = data(pos+1:pos+stretched_period).*(1+cos(dphase*(0:stretched_period-1)'))*0.5;      
        rise = data(pos-stretched_period+1:pos).*(1-cos(dphase*(0:stretched_period-1)'))*0.5;
        fprintf(debug_file_id,'start_rise:%d end_rise:%d\n',pos-stretched_period+1,pos);
        fprintf(debug_file_id,'start_fall:%d end_fall:%d\n',pos+1,pos+stretched_period);
        src_pos = src_pos + period;
        stretched_pos = floor(src_pos*ratio);
        delta_pos = stretched_pos - tgt_pos;
        tgt_pk_pos = output_len-last_fall_len+delta_pos;
        tgt_pk_pos = output_len;
        output(tgt_pk_pos-stretched_period+1:tgt_pk_pos) = output(tgt_pk_pos-stretched_period+1:tgt_pk_pos) + rise;
        output(tgt_pk_pos+1:tgt_pk_pos+stretched_period) = output(tgt_pk_pos+1:tgt_pk_pos+stretched_period) + fall;
        output_len = tgt_pk_pos+stretched_period;
        last_fall_len = floor(stretched_period);
    end
    tgt_acc_len = stretched_pos;
    src_acc_len = src_pos;
%     if (output_len - stretched_period)<stretched_period
%         joint_buf_len = output_len - stretched_period;
%     else
%         joint_buf_len = stretched_period;
%     end
    joint_buf_len = last_joint_buf_len;
    fall_buf_len = floor(stretched_period);
    peak = floor(pos);
    unvoiced_pos = floor(pos);                                                        
%     pos = LastPeakPos;
%     noise_period = 0.008*44100;%noise period is set to 0.008s
%     period = floor(noise_period*ratio);
%     dphase = pi/period;
%     num = floor((prcs_end - LastPeakPos)/noise_period);   
%     if num==0
%         output_len = 0;
%         peak = LastPeakPos;
%         output = [];
%         return;
%     end
%     output = zeros(3*ShiftLen*2,1);
%     write_ptr = 0;
%     
%     for i=1:num
%         fall = data(pos+1:pos+period).*(1+cos(dphase*(0:period-1)'))*0.5;
%         pos = pos + i*floor(noise_period);
%         rise = data(pos-period+1:pos).*(1-cos(dphase*(0:period-1)'))*0.5;
%         output(write_ptr+1:write_ptr+period) = fall + rise;
%         write_ptr = write_ptr+period; 
%     end
% 
%     output_len = write_ptr;
%     peak = floor(pos);
end

function [peak,output,output_len,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,last_src_acc_pos] = process_one_voiced_interval(data,LastPeakPos,LastPeakFreq,peak_seq,freq_seq,peak_count,...
                                                                                                                    ratio,noise_period,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                    last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id)
    tgt_pos = TargtAccLen;
    src_pos = SourceAccLen;
    pos = LastPeakPos;
    if LastPeakFreq==0
        stretched_period = floor(noise_period*ratio);
        period = floor(noise_period);
    else
        stretched_period = floor(Fs/LastPeakFreq*ratio);
        period = floor(Fs/LastPeakFreq);
    end   
%     if freq_seq(1)==0
%         stretched_period = noise_period*ratio;
%         period = noise_period;
%     else
%         stretched_period = floor(Fs/freq_seq(1)*ratio);
%         period = floor(Fs/freq_seq(1));
%     end 
    output = zeros(3*ShiftLen*2+last_joint_buf_len,1);
    output_len = 0;
    output(output_len+1:output_len+last_joint_buf_len) = last_joint_buf;
    output_len = output_len + last_joint_buf_len;
    output(output_len+1:output_len+last_fall_len) = last_fall_buf;
    output_len = output_len + last_fall_len;

    interval_peak = src_acc_pos + period;
    if interval_peak>peak_seq(peak_count)
        tgt_acc_len = TargtAccLen;
        src_acc_len = SourceAccLen;
        joint_buf_len = last_joint_buf_len;
        fall_buf_len = last_fall_len;
        last_src_acc_pos = src_acc_pos;
        peak = LastPeakPos;
        return;
    end
    stretched_pos = TargtAccLen;
%     if interval_peak>peak_seq(peak_count)
%         peak = LastPeakPos;
%         tgt_acc_len = TargtAccLen;
%         src_acc_len = SourceAccLen;
%         joint_buf_len = last_joint_buf_len;
%         fall_buf_len = last_fall_len;
%         return;
%     end
    
    last_fall_len = 0;
    for i=1:peak_count
        dphase = pi/stretched_period;
%         dphase = 1/stretched_period;

        while interval_peak<=peak_seq(i)
%             left_dist = interval_peak - pos;
%             right_dist = peak_seq(i)- interval_peak;
%             if left_dist<=right_dist%choose left peak 
%                 pitch_period = floor(left_pitch_period*ratio);
%                 fall = data(pos+1:pos+pitch_period).*(1+cos(dphase_left*(0:pitch_period-1)'))*0.5;
%                 rise = data(pos-pitch_period+1:pos).*(1-cos(dphase_left*(0:pitch_period-1)'))*0.5;
%                 output(output_len+1:output_len+pitch_period) = fall + rise;
%                 output_len = output_len + pitch_period;
%             else 
%                 pitch_period = floor(right_pitch_period*ratio);
%                 fall = data(peak_seq(i)+1:peak_seq(i)+pitch_period).*(1+cos(dphase_right*(0:pitch_period-1)'))*0.5;
%                 rise = data(peak_seq(i)-pitch_period+1:peak_seq(i)).*(1-cos(dphase_right*(0:pitch_period-1)'))*0.5;
%                 output(output_len+1:output_len+pitch_period) = fall + rise;
%                 output_len = output_len + pitch_period;
%             end
%             win = blackman(stretched_period*2);
%             win_fall = win(stretched_period+1:end);
%             win_rise = win(1:stretched_period);
%             win_fall = win_fall(:);
%             win_rise = win_rise(:);
            fall = data(pos+1:pos+stretched_period).*(1+cos(dphase*(0:stretched_period-1)'))*0.5;           
            rise = data(pos-stretched_period+1:pos).*(1-cos(dphase*(0:stretched_period-1)'))*0.5;
%             fall = data(pos+1:pos+stretched_period).*win_fall;           
%             rise = data(pos-stretched_period+1:pos).*win_rise;
%             fall = data(pos+1:pos+stretched_period).*(stretched_period:-1:1)'*dphase;           
%             rise = data(pos-stretched_period+1:pos).*(1:stretched_period)'*dphase;
            fprintf(debug_file_id,'r:%d-%d\n',pos-stretched_period+1,pos);
            fprintf(debug_file_id,'f:%d-%d\n',pos+1,pos+stretched_period);
            src_pos = src_pos + period;
%             stretched_pos = floor(src_pos*ratio);
%             delta_pos = stretched_pos - tgt_pos;
%             tgt_pk_pos = output_len-last_fall_len+delta_pos;
            tgt_pk_pos = output_len;
            output(tgt_pk_pos-stretched_period+1:tgt_pk_pos) = output(tgt_pk_pos-stretched_period+1:tgt_pk_pos) + rise;
            output(tgt_pk_pos+1:tgt_pk_pos+stretched_period) = output(tgt_pk_pos+1:tgt_pk_pos+stretched_period) + fall;
            output_len = tgt_pk_pos+stretched_period;
            
            last_fall_len = floor(stretched_period);
                
            interval_peak = interval_peak + period;           
        end
        pos = peak_seq(i); 
        if freq_seq(i)==0
            stretched_period = floor(noise_period*ratio);
            period = floor(noise_period);
        else
            stretched_period = floor(Fs/freq_seq(i)*ratio);
            period = floor(Fs/freq_seq(i));
        end 
        
%         pulse_pos = get_peak_from_position(pos,peak_seq(i),peak_seq(peak_count));
%         fall = data(pos+1:pos+fall_period).*(1+cos(dphase_fall*(0:fall_period-1)'))*0.5;
%         if freq_seq(i)==0
%             rise_period = floor(noise_period*ratio);
%         else
%             rise_period = floor(Fs/freq_seq(i)*ratio);
%         end       
%         dphase_rise = pi/rise_period;
%         rise = data(peak_seq(i)-rise_period+1:peak_seq(i)).*(1-cos(dphase_rise*(0:rise_period-1)'))*0.5;
%         if fall_period>rise_period
%             output(write_ptr+1:write_ptr+fall_period) = fall;
%             output(write_ptr+fall_period-rise_period+1:write_ptr+fall_period) = output(write_ptr+fall_period-rise_period+1:write_ptr+fall_period) + rise;
%             write_ptr = write_ptr+fall_period;
%         else
%             output(write_ptr+1:write_ptr+fall_period) = fall;
%             output(write_ptr+1:write_ptr+rise_period) = output(write_ptr+1:write_ptr+rise_period) + rise;
%             write_ptr = write_ptr+rise_period;
%         end
%         pos = peak_seq(i);
%         fall_period = rise_period;
    end
    tgt_acc_len = stretched_pos;
    src_acc_len = src_pos;
%     if (output_len - stretched_period)<stretched_period
%         joint_buf_len = output_len - stretched_period;
%     else
%         joint_buf_len = stretched_period;
%     end
    joint_buf_len = last_joint_buf_len;
    fall_buf_len = last_fall_len;
%     
%     output_len = write_ptr;
    peak = peak_seq(peak_count);
    last_src_acc_pos = interval_peak - period;
end

function [output_uv,output_len_uv,tgt_acc_len,src_acc_len,joint_buf_len,fall_buf_len,unvoiced_pos] = process_unvoiced_to_voiced_interval(data,LastPeakPos,LocalPeakPos,LocalFreq,...
                                                                                                                            noise_period,ratio,ShiftLen,Fs,last_joint_buf,last_joint_buf_len,...
                                                                                                                            last_fall_buf,last_fall_len,TargtAccLen,SourceAccLen,src_acc_pos,debug_file_id)
%     tgt_pos = TargtAccLen;
    src_pos = SourceAccLen;
    pos = src_acc_pos;
    stretched_period = floor(noise_period*ratio);
    period = floor(noise_period);
  
    output_uv = zeros(3*ShiftLen*2+last_joint_buf_len,1);
    output_len_uv = 0;
    output_uv(output_len_uv+1:output_len_uv+last_joint_buf_len) = last_joint_buf;
    output_len_uv = output_len_uv + last_joint_buf_len;
    output_uv(output_len_uv+1:output_len_uv+last_fall_len) = last_fall_buf;
    output_len_uv = output_len_uv + last_fall_len;

    if (LocalPeakPos-src_acc_pos)<=period
        period = LocalPeakPos-src_acc_pos;
        stretched_period = floor(period*ratio);
    end
    interval_peak = pos + period; 
    stretched_pos = 0;

    dphase = pi/stretched_period;

    while interval_peak<=LocalPeakPos
        fall = data(pos+1:pos+stretched_period).*(1+cos(dphase*(0:stretched_period-1)'))*0.5;      
        rise = data(pos-stretched_period+1:pos).*(1-cos(dphase*(0:stretched_period-1)'))*0.5;
        fprintf(debug_file_id,'start_rise:%d end_rise:%d\n',pos-stretched_period+1,pos);
        fprintf(debug_file_id,'start_fall:%d end_fall:%d\n',pos+1,pos+stretched_period);
%         fprintf(debug_file_id,'%d\n',pos+1,2*stretched_period);
        src_pos = src_pos + period;
        stretched_pos = floor(src_pos*ratio);
%         delta_pos = stretched_pos - tgt_pos;
%         tgt_pk_pos = output_len_uv-last_fall_len+delta_pos;
        tgt_pk_pos = output_len_uv;
        output_uv(tgt_pk_pos-stretched_period+1:tgt_pk_pos) = output_uv(tgt_pk_pos-stretched_period+1:tgt_pk_pos) + rise;
        output_uv(tgt_pk_pos+1:tgt_pk_pos+stretched_period) = output_uv(tgt_pk_pos+1:tgt_pk_pos+stretched_period) + fall;
        output_len_uv = tgt_pk_pos+stretched_period;

%         last_fall_len = floor(stretched_period);

        interval_peak = interval_peak + period;           
    end

    tgt_acc_len = stretched_pos;
    src_acc_len = src_pos;
%     if (output_len_uv - stretched_period)<stretched_period
%         joint_buf_len = output_len_uv - stretched_period;
%     else
%         joint_buf_len = stretched_period;
%     end
    joint_buf_len = last_joint_buf_len;
    fall_buf_len = floor(stretched_period);
    unvoiced_pos = LocalPeakPos;
end



