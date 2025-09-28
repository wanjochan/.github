// Main JavaScript for QuantTech Finance Blog

document.addEventListener('DOMContentLoaded', function() {
    initNavigation();
    initScrollEffects();
    initAnimations();
});

// Navigation functionality
function initNavigation() {
    const navToggle = document.getElementById('navToggle');
    const navMenu = document.getElementById('navMenu');
    
    if (navToggle && navMenu) {
        navToggle.addEventListener('click', function() {
            navMenu.classList.toggle('active');
            navToggle.classList.toggle('active');
        });
    }
    
    // Close mobile menu when clicking on a link
    const navLinks = document.querySelectorAll('.nav-link');
    navLinks.forEach(link => {
        link.addEventListener('click', function() {
            navMenu.classList.remove('active');
            navToggle.classList.remove('active');
        });
    });
}

// Scroll effects and animations
function initScrollEffects() {
    // Intersection Observer for fade-in animations
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -50px 0px'
    };
    
    const observer = new IntersectionObserver(function(entries) {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.classList.add('animate-fade-in-up');
                observer.unobserve(entry.target);
            }
        });
    }, observerOptions);
    
    // Observe elements for animation
    const animateElements = document.querySelectorAll('.feature-card, .post-card, .stat-item');
    animateElements.forEach(el => observer.observe(el));
}

// Initialize animations
function initAnimations() {
    // Counter animation for stats
    const statValues = document.querySelectorAll('.stat-value');
    statValues.forEach(stat => {
        const text = stat.textContent;
        const number = parseFloat(text.replace(/[^\d.-]/g, ''));
        
        if (!isNaN(number)) {
            animateCounter(stat, 0, number, 2000, text);
        }
    });
}

// Counter animation function
function animateCounter(element, start, end, duration, originalText) {
    const startTime = performance.now();
    const isPercentage = originalText.includes('%');
    const isPositive = originalText.includes('+');
    const isNegative = originalText.includes('-');
    
    function updateCounter(currentTime) {
        const elapsed = currentTime - startTime;
        const progress = Math.min(elapsed / duration, 1);
        
        const current = start + (end - start) * easeOutQuart(progress);
        let displayValue = current.toFixed(1);
        
        if (isPercentage) displayValue += '%';
        if (isPositive && current > 0) displayValue = '+' + displayValue;
        if (isNegative && current < 0) displayValue = '-' + Math.abs(displayValue);
        
        element.textContent = displayValue;
        
        if (progress < 1) {
            requestAnimationFrame(updateCounter);
        } else {
            element.textContent = originalText;
        }
    }
    
    requestAnimationFrame(updateCounter);
}

// Easing function
function easeOutQuart(t) {
    return 1 - (--t) * t * t * t;
}

// Charts removed for blog-focused mobile design
