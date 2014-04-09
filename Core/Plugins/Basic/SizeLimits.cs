﻿/* Copyright (c) 2014 Imazen See license.txt */
using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using ImageResizer.Configuration;
using ImageResizer.Configuration.Xml;
using ImageResizer.Configuration.Issues;
using ImageResizer.Resizing;

namespace ImageResizer.Plugins.Basic {
    /// <summary>
    /// Enforces two kinds of maximum size constraints: a Total size limit, and an Image size limit. 
    /// Image limits do not include padding, border, or effect widths, and automatically shrink the 'width/height/maxwidth/maxheight' values to fit within the limits.
    /// Total limits are absolute and apply to the actual result dimensions, causing an exception if they are exceeded. 
    /// Limits are checked during the layout phase,  prior to resource-intensive bitmap operations.
    /// </summary>
    public class SizeLimits:IssueSink {

        public class SizeLimitException : ImageProcessingException {
            public SizeLimitException(string message) : base(message) { }
        }
        public SizeLimits():base("SizeLimits") {
        }
        public SizeLimits(Config c):base("SizeLimits") {
            //<sizelimits imageWidth="" imageHeight="" totalWidth="" totalHeight="" totalBehavior="ignorelimits|throwexception" />

            int imgWidth =  c.get("sizelimits.imageWidth", imageSize.Width);
            int imgHeight = c.get("sizelimits.imageHeight", imageSize.Height);
            imageSize = new Size(imgWidth, imgHeight);

            int totalWidth = Math.Max(1, c.get("sizelimits.totalWidth", totalSize.Width));
            int totalHeight = Math.Max(1, c.get("sizelimits.totalHeight", totalSize.Height));
            if (totalWidth < 1 || totalHeight < 1)
                AcceptIssue(new Issue("sizelimits.totalWidth and sizelimits.totalHeight must both be greater than 0. Reverting to defaults.", IssueSeverity.ConfigurationError));
            else
                totalSize = new Size(totalWidth, totalHeight);


            totalBehavior =  c.get<TotalSizeBehavior>("sizelimits.totalbehavior", TotalSizeBehavior.ThrowException);
           
        }

        public void ValidateTotalSize(Size total) {
            if (TotalBehavior == TotalSizeBehavior.ThrowException && !FitsInsideTotalSize(total)) 
                throw new SizeLimitException("The dimensions of the output image (" + total.Width + "x" + total.Height +
                                ") exceed the configured maximum dimensions of " + TotalSize.Width + "x" + TotalSize.Height + ". You can change these limits in Web.config through the SizeLimiting plugin.");
        }


        public bool FitsInsideTotalSize(Size s) {
            return (s.Width <= totalSize.Width && s.Height <= totalSize.Height);
        }

        public enum TotalSizeBehavior {
            ThrowException,
            IgnoreLimits
        }
        protected Size totalSize = new Size(3200,3200);
        /// <summary>
        /// The maximum final size an image generated by ImageBuilder can be.  
        /// Defaults to 3200x3200
        /// </summary>
        public Size TotalSize {
            get { return totalSize; }
            set {  totalSize = value; }
        }
        protected TotalSizeBehavior totalBehavior = TotalSizeBehavior.ThrowException;
        /// <summary>
        /// What action to take when the total size of the final image would exceed the TotalSize value.
        /// Defaults to ThowException
        /// </summary>
        public TotalSizeBehavior TotalBehavior {
            get { return totalBehavior; }
            set {   totalBehavior = value; }
        }

        /// <summary>
        /// Returns true if ImageSize is specified.
        /// </summary>
        public bool HasImageSize { get { return ImageSize.Width > 0 && ImageSize.Height > 0; } }

        protected Size imageSize = new Size(0,0);
        /// <summary>
        /// The maximum size an un-rotated image can be drawn when creating a resized image. 
        /// Rotation will increase the total size, as will any borders, paddings, margins, or
        /// effects. Not effective at preventing attacks, use totalSize.
        /// If larger values are specified in a querystring, they will automatically
        /// be scaled to fit within these dimensions.
        /// Defaults to 0x0, which means no limits
        /// </summary>
        public Size ImageSize {
            get { return imageSize; }
            set { imageSize = value; }
        }



    }
}
